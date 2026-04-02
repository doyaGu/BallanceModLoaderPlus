from __future__ import annotations


RS_DATA = 14
RS_CODE = 30
RS_PARITY = 16


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


GF_EXP = [0] * 512
GF_LOG = [0] * 256


def _init_gf_tables() -> None:
    x = 1
    for i in range(255):
        GF_EXP[i] = x
        GF_LOG[x] = i
        x <<= 1
        if x & 0x100:
            x ^= 0x11D
    for i in range(255, 512):
        GF_EXP[i] = GF_EXP[i - 255]


_init_gf_tables()


def gf_mul(a: int, b: int) -> int:
    if a == 0 or b == 0:
        return 0
    return GF_EXP[GF_LOG[a] + GF_LOG[b]]


def gf_pow(base: int, exp: int) -> int:
    if base == 0:
        return 0
    return GF_EXP[(GF_LOG[base] * exp) % 255]


def gf_inv(a: int) -> int:
    if a == 0:
        return 0
    return GF_EXP[255 - GF_LOG[a]]


def rs_build_generator() -> list[int]:
    generator = [0] * (RS_PARITY + 1)
    generator[0] = 1
    generator_len = 1

    for i in range(RS_PARITY):
        root = gf_pow(2, i)
        tmp = [0] * (RS_PARITY + 1)
        for j in range(generator_len):
            tmp[j] ^= gf_mul(generator[j], root)
            tmp[j + 1] ^= generator[j]
        generator_len += 1
        generator[:generator_len] = tmp[:generator_len]

    return generator


RS_GENERATOR = rs_build_generator()


def rs_encode(data: list[int]) -> list[int]:
    message = list(data[:RS_DATA])
    if len(message) != RS_DATA:
        raise ValueError(f"Expected {RS_DATA} data bytes, got {len(message)}")

    out = message + [0] * RS_PARITY
    for i in range(RS_DATA):
        feedback = out[i]
        if feedback == 0:
            continue
        for j in range(1, RS_PARITY + 1):
            out[i + j] ^= gf_mul(RS_GENERATOR[RS_PARITY - j], feedback)

    out[:RS_DATA] = message
    return out


def rs_calc_syndromes(message: list[int]) -> list[int]:
    syndromes = [0] * RS_PARITY
    for i in range(RS_PARITY):
        syndrome = 0
        for j in range(RS_CODE):
            syndrome = gf_mul(syndrome, gf_pow(2, i)) ^ message[j]
        syndromes[i] = syndrome
    return syndromes


def rs_has_errors(syndromes: list[int]) -> bool:
    return any(value != 0 for value in syndromes)


def rs_berlekamp_massey(syndromes: list[int]) -> list[int]:
    n = len(syndromes)
    c_poly = [0] * (n + 1)
    b_poly = [0] * (n + 1)
    c_poly[0] = 1
    b_poly[0] = 1
    length = 0
    shift = 1
    discrepancy_base = 1

    for n_iter in range(n):
        discrepancy = syndromes[n_iter]
        for i in range(1, length + 1):
            discrepancy ^= gf_mul(c_poly[i], syndromes[n_iter - i])
        if discrepancy == 0:
            shift += 1
            continue

        if 2 * length <= n_iter:
            old = c_poly[:]
            coeff = gf_mul(discrepancy, gf_inv(discrepancy_base))
            for i in range(shift, n + 1):
                c_poly[i] ^= gf_mul(coeff, b_poly[i - shift])
            length = n_iter + 1 - length
            b_poly = old
            discrepancy_base = discrepancy
            shift = 1
            continue

        coeff = gf_mul(discrepancy, gf_inv(discrepancy_base))
        for i in range(shift, n + 1):
            c_poly[i] ^= gf_mul(coeff, b_poly[i - shift])
        shift += 1

    return c_poly[: length + 1]


def rs_find_errors(err_loc: list[int]) -> list[int]:
    positions = []
    for i in range(255):
        value = 0
        for j, coeff in enumerate(err_loc):
            value ^= gf_mul(coeff, gf_pow(gf_pow(2, i), j))
        if value == 0:
            pos = (i + RS_CODE - 1) % 255
            if 0 <= pos < RS_CODE:
                positions.append(pos)
    return positions


def rs_solve_error_magnitudes(syndromes: list[int], err_pos: list[int]) -> list[int] | None:
    """Solve error magnitudes via Vandermonde system on syndromes.

    S_i = sum_j e_j * A_j^i where A_j = alpha^(n-1-pos_j) for descending polynomial.
    """
    v = len(err_pos)
    if v == 0:
        return []

    # A_j = alpha^(RS_CODE - 1 - pos_j) for each error position
    a_vals = [gf_pow(2, RS_CODE - 1 - pos) for pos in err_pos]

    # Build v x (v+1) augmented matrix [Vandermonde | syndromes]
    matrix = []
    for i in range(v):
        row = [gf_pow(a_vals[j], i) for j in range(v)]
        row.append(syndromes[i])
        matrix.append(row)

    # Gaussian elimination in GF(2^8)
    for col in range(v):
        # Find pivot
        pivot = -1
        for row in range(col, v):
            if matrix[row][col] != 0:
                pivot = row
                break
        if pivot == -1:
            return None
        if pivot != col:
            matrix[col], matrix[pivot] = matrix[pivot], matrix[col]

        inv_pivot = gf_inv(matrix[col][col])
        for k in range(col, v + 1):
            matrix[col][k] = gf_mul(matrix[col][k], inv_pivot)

        for row in range(v):
            if row == col or matrix[row][col] == 0:
                continue
            factor = matrix[row][col]
            for k in range(col, v + 1):
                matrix[row][k] ^= gf_mul(factor, matrix[col][k])

    return [matrix[j][v] for j in range(v)]


def rs_decode(data: list[int]) -> list[int] | None:
    message = list(data[:RS_CODE])
    if len(message) != RS_CODE:
        return None

    if rs_encode(message[:RS_DATA]) == message:
        return message[:RS_DATA]

    syndromes = rs_calc_syndromes(message)
    if not rs_has_errors(syndromes):
        return message[:RS_DATA]

    err_loc = rs_berlekamp_massey(syndromes)
    err_pos = rs_find_errors(err_loc)
    if len(err_pos) != len(err_loc) - 1:
        return None
    if len(err_pos) > RS_PARITY // 2:
        return None

    magnitudes = rs_solve_error_magnitudes(syndromes, err_pos)
    if magnitudes is None:
        return None
    for pos, mag in zip(err_pos, magnitudes):
        message[pos] ^= mag

    if rs_has_errors(rs_calc_syndromes(message)):
        return None
    return message[:RS_DATA]
