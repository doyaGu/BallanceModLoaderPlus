#include "imgui.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

bool ImGui_ImplCK2_TestBuildDrawSegment(const ImDrawCmd *commands, int command_count,
                                        int first_command, int vertex_count,
                                        unsigned int *segment_vertex_offset,
                                        unsigned int *segment_vertex_count,
                                        int *end_command);
bool ImGui_ImplCK2_TestShouldUploadDrawSegment(const ImDrawCmd *commands, int command_count,
                                               const ImDrawIdx *indices, int index_count,
                                               int first_command, int vertex_count);

namespace Benchmark {
    constexpr unsigned int MaxIndexedVertices = 0x10000U;
    constexpr unsigned int LegacySliceThreshold = 0xFFFFU;
    constexpr int SampleCount = 9;
    constexpr std::chrono::milliseconds CalibrationTime(30);
    constexpr std::chrono::milliseconds SampleTime(60);

#if defined(_MSC_VER)
#define BML_BENCHMARK_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define BML_BENCHMARK_NOINLINE __attribute__((noinline))
#else
#define BML_BENCHMARK_NOINLINE
#endif

    struct Position {
        float X;
        float Y;
        float Z;
        float W;
    };

    struct TexCoord {
        float U;
        float V;
    };

    struct UploadedVertex {
        Position PositionData;
        ImU32 Color;
        TexCoord TexCoordData;
    };

    static_assert(sizeof(UploadedVertex) == 28, "CK2 interleaved vertex layout changed");

    struct Scene {
        std::string Name;
        std::vector<ImDrawVert> Vertices;
        std::vector<ImDrawIdx> Indices;
        std::vector<ImDrawCmd> Commands;
    };

    struct WorkingSet {
        std::vector<UploadedVertex> UploadedVertices;
        std::vector<Position> Positions;
        std::vector<ImU32> Colors;
        std::vector<TexCoord> TexCoords;
        std::vector<ImDrawIdx> RebasedIndices;

        WorkingSet()
            : UploadedVertices(MaxIndexedVertices), Positions(MaxIndexedVertices),
              Colors(MaxIndexedVertices), TexCoords(MaxIndexedVertices) {}
    };

    struct PreparationStats {
        std::uint64_t Checksum = 0;
        std::uint64_t UploadedVertexCount = 0;
        std::uint64_t RebasedIndexCount = 0;
        unsigned int UploadCount = 0;
        unsigned int CallCount = 0;
    };

    struct TimingResult {
        double MedianNanoseconds = 0.0;
        double MinimumNanoseconds = 0.0;
        double MaximumNanoseconds = 0.0;
    };

    struct ComparisonResult {
        TimingResult BaselineTiming;
        TimingResult CandidateTiming;
        PreparationStats BaselineStats;
        PreparationStats CandidateStats;
    };

    using PreparationFunction = PreparationStats (*)(const Scene &, WorkingSet &);

    std::atomic<std::uint64_t> ResultSink = 0;
    bool ValidationFailed = false;

    std::uint32_t FloatBits(float value) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    BML_BENCHMARK_NOINLINE
    std::uint64_t ObserveUpload(const UploadedVertex *vertices, unsigned int vertex_count) {
        if (!vertices || vertex_count == 0)
            return 0;

        const UploadedVertex &first = vertices[0];
        const UploadedVertex &middle = vertices[vertex_count / 2];
        const UploadedVertex &last = vertices[vertex_count - 1];
        return (std::uint64_t)first.Color + ((std::uint64_t)middle.Color << 1U) +
               ((std::uint64_t)last.Color << 2U) + FloatBits(first.PositionData.X) +
               FloatBits(middle.PositionData.Y) + FloatBits(last.TexCoordData.U) + vertex_count;
    }

    BML_BENCHMARK_NOINLINE
    std::uint64_t ObservePlanar(const WorkingSet &working_set, unsigned int vertex_count) {
        if (vertex_count == 0)
            return 0;

        const unsigned int middle = vertex_count / 2;
        const unsigned int last = vertex_count - 1;
        return (std::uint64_t)working_set.Colors[0] +
               ((std::uint64_t)working_set.Colors[middle] << 1U) +
               ((std::uint64_t)working_set.Colors[last] << 2U) +
               FloatBits(working_set.Positions[0].X) +
               FloatBits(working_set.Positions[middle].Y) +
               FloatBits(working_set.TexCoords[last].U) + vertex_count;
    }

    std::uint64_t UploadVertices(const Scene &scene, unsigned int vertex_offset,
                                 unsigned int vertex_count, WorkingSet &working_set) {
        UploadedVertex *destination = working_set.UploadedVertices.data();
        const ImDrawVert *source = scene.Vertices.data() + vertex_offset;
        for (unsigned int i = 0; i < vertex_count; ++i) {
            destination[i].PositionData.X = source[i].pos.x;
            destination[i].PositionData.Y = source[i].pos.y;
            destination[i].PositionData.Z = 0.0f;
            destination[i].PositionData.W = 1.0f;
            destination[i].Color = source[i].col;
            destination[i].TexCoordData.U = source[i].uv.x;
            destination[i].TexCoordData.V = source[i].uv.y;
        }
        return ObserveUpload(destination, vertex_count);
    }

    BML_BENCHMARK_NOINLINE
    void PackStrided(const ImDrawVert *source, unsigned int vertex_count,
                     void *position_data, unsigned int position_stride,
                     void *color_data, unsigned int color_stride,
                     void *tex_coord_data, unsigned int tex_coord_stride) {
        unsigned char *position = (unsigned char *)position_data;
        unsigned char *color = (unsigned char *)color_data;
        unsigned char *tex_coord = (unsigned char *)tex_coord_data;
        for (unsigned int i = 0; i < vertex_count; ++i) {
            Position *output_position = (Position *)position;
            output_position->X = source[i].pos.x;
            output_position->Y = source[i].pos.y;
            output_position->Z = 0.0f;
            output_position->W = 1.0f;
            *(ImU32 *)color = source[i].col;
            TexCoord *output_tex_coord = (TexCoord *)tex_coord;
            output_tex_coord->U = source[i].uv.x;
            output_tex_coord->V = source[i].uv.y;
            position += position_stride;
            color += color_stride;
            tex_coord += tex_coord_stride;
        }
    }

    BML_BENCHMARK_NOINLINE
    void PackInterleaved(const ImDrawVert *source, unsigned int vertex_count,
                         UploadedVertex *destination) {
        for (unsigned int i = 0; i < vertex_count; ++i) {
            destination[i].PositionData.X = source[i].pos.x;
            destination[i].PositionData.Y = source[i].pos.y;
            destination[i].PositionData.Z = 0.0f;
            destination[i].PositionData.W = 1.0f;
            destination[i].Color = source[i].col;
            destination[i].TexCoordData.U = source[i].uv.x;
            destination[i].TexCoordData.V = source[i].uv.y;
        }
    }

    BML_BENCHMARK_NOINLINE
    void PackPlanar(const ImDrawVert *source, unsigned int vertex_count,
                    Position *positions, ImU32 *colors, TexCoord *tex_coords) {
        for (unsigned int i = 0; i < vertex_count; ++i) {
            positions[i].X = source[i].pos.x;
            positions[i].Y = source[i].pos.y;
            positions[i].Z = 0.0f;
            positions[i].W = 1.0f;
            colors[i] = source[i].col;
            tex_coords[i].U = source[i].uv.x;
            tex_coords[i].V = source[i].uv.y;
        }
    }

    PreparationStats PrepareStridedInterleaved(const Scene &scene, WorkingSet &working_set) {
        PreparationStats stats;
        if (scene.Vertices.empty())
            return stats;
        const unsigned int vertex_count = (unsigned int)scene.Vertices.size();
        UploadedVertex *vertices = working_set.UploadedVertices.data();
        PackStrided(scene.Vertices.data(), vertex_count,
                    &vertices[0].PositionData, sizeof(UploadedVertex),
                    &vertices[0].Color, sizeof(UploadedVertex),
                    &vertices[0].TexCoordData, sizeof(UploadedVertex));
        stats.Checksum = ObserveUpload(vertices, vertex_count);
        stats.UploadedVertexCount = vertex_count;
        return stats;
    }

    PreparationStats PrepareTypedInterleaved(const Scene &scene, WorkingSet &working_set) {
        PreparationStats stats;
        if (scene.Vertices.empty())
            return stats;
        const unsigned int vertex_count = (unsigned int)scene.Vertices.size();
        UploadedVertex *vertices = working_set.UploadedVertices.data();
        PackInterleaved(scene.Vertices.data(), vertex_count, vertices);
        stats.Checksum = ObserveUpload(vertices, vertex_count);
        stats.UploadedVertexCount = vertex_count;
        return stats;
    }

    PreparationStats PrepareStridedPlanar(const Scene &scene, WorkingSet &working_set) {
        PreparationStats stats;
        if (scene.Vertices.empty())
            return stats;
        const unsigned int vertex_count = (unsigned int)scene.Vertices.size();
        PackStrided(scene.Vertices.data(), vertex_count,
                    working_set.Positions.data(), sizeof(Position),
                    working_set.Colors.data(), sizeof(ImU32),
                    working_set.TexCoords.data(), sizeof(TexCoord));
        stats.Checksum = ObservePlanar(working_set, vertex_count);
        stats.UploadedVertexCount = vertex_count;
        return stats;
    }

    PreparationStats PrepareTypedPlanar(const Scene &scene, WorkingSet &working_set) {
        PreparationStats stats;
        if (scene.Vertices.empty())
            return stats;
        const unsigned int vertex_count = (unsigned int)scene.Vertices.size();
        PackPlanar(scene.Vertices.data(), vertex_count, working_set.Positions.data(),
                   working_set.Colors.data(), working_set.TexCoords.data());
        stats.Checksum = ObservePlanar(working_set, vertex_count);
        stats.UploadedVertexCount = vertex_count;
        return stats;
    }

    bool BuildDrawSlice(const Scene &scene, const ImDrawCmd &command, WorkingSet &working_set,
                        unsigned int &vertex_offset, unsigned int &vertex_count) {
        if (command.ElemCount == 0 || command.IdxOffset > scene.Indices.size() ||
            command.ElemCount > scene.Indices.size() - command.IdxOffset)
            return false;

        const ImDrawIdx *indices = scene.Indices.data() + command.IdxOffset;
        ImDrawIdx minimum = indices[0];
        ImDrawIdx maximum = indices[0];
        for (unsigned int i = 1; i < command.ElemCount; ++i) {
            minimum = std::min(minimum, indices[i]);
            maximum = std::max(maximum, indices[i]);
        }

        const std::uint64_t first_vertex = (std::uint64_t)command.VtxOffset + minimum;
        const std::uint64_t last_vertex = (std::uint64_t)command.VtxOffset + maximum;
        if (last_vertex >= scene.Vertices.size())
            return false;

        const std::uint64_t slice_vertex_count = last_vertex - first_vertex + 1;
        if (slice_vertex_count > MaxIndexedVertices)
            return false;

        working_set.RebasedIndices.resize(command.ElemCount);
        for (unsigned int i = 0; i < command.ElemCount; ++i)
            working_set.RebasedIndices[i] = (ImDrawIdx)(indices[i] - minimum);

        vertex_offset = (unsigned int)first_vertex;
        vertex_count = (unsigned int)slice_vertex_count;
        return true;
    }

    PreparationStats PrepareOld(const Scene &scene, WorkingSet &working_set) {
        PreparationStats stats;
        bool use_command_slices = scene.Vertices.size() >= LegacySliceThreshold;
        for (const ImDrawCmd &command : scene.Commands)
            use_command_slices = use_command_slices || command.VtxOffset != 0;

        if (!use_command_slices) {
            const unsigned int vertex_count = (unsigned int)scene.Vertices.size();
            stats.Checksum = UploadVertices(scene, 0, vertex_count, working_set);
            stats.UploadCount = 1;
            stats.UploadedVertexCount = vertex_count;
            return stats;
        }

        for (const ImDrawCmd &command : scene.Commands) {
            unsigned int vertex_offset = 0;
            unsigned int vertex_count = 0;
            if (!BuildDrawSlice(scene, command, working_set, vertex_offset, vertex_count))
                continue;

            stats.Checksum += UploadVertices(scene, vertex_offset, vertex_count, working_set);
            stats.UploadedVertexCount += vertex_count;
            stats.RebasedIndexCount += command.ElemCount;
            ++stats.UploadCount;
        }
        return stats;
    }

    PreparationStats PrepareCurrent(const Scene &scene, WorkingSet &working_set) {
        PreparationStats stats;
        bool use_single_upload = !scene.Vertices.empty() &&
                                 scene.Vertices.size() <= MaxIndexedVertices;
        for (const ImDrawCmd &command : scene.Commands)
            use_single_upload = use_single_upload && command.VtxOffset == 0;

        if (use_single_upload) {
            const unsigned int vertex_count = (unsigned int)scene.Vertices.size();
            stats.Checksum = UploadVertices(scene, 0, vertex_count, working_set);
            stats.UploadCount = 1;
            stats.UploadedVertexCount = vertex_count;
            return stats;
        }

        unsigned int segment_vertex_offset = 0;
        unsigned int segment_vertex_count = 0;
        int segment_end_command = 0;
        bool segment_valid = false;
        bool upload_whole_segment = false;
        bool segment_uploaded = false;

        for (int command_index = 0; command_index < (int)scene.Commands.size(); ++command_index) {
            const ImDrawCmd &command = scene.Commands[(std::size_t)command_index];
            if (command_index >= segment_end_command) {
                segment_valid = ImGui_ImplCK2_TestBuildDrawSegment(
                    scene.Commands.data(), (int)scene.Commands.size(), command_index,
                    (int)scene.Vertices.size(), &segment_vertex_offset,
                    &segment_vertex_count, &segment_end_command);
                if (!segment_valid)
                    segment_end_command = command_index + 1;
                upload_whole_segment = segment_valid &&
                    ImGui_ImplCK2_TestShouldUploadDrawSegment(
                        scene.Commands.data(), (int)scene.Commands.size(), scene.Indices.data(),
                        (int)scene.Indices.size(), command_index, (int)scene.Vertices.size());
                segment_uploaded = false;
            }

            if (command.ElemCount == 0 || command.IdxOffset > scene.Indices.size() ||
                command.ElemCount > scene.Indices.size() - command.IdxOffset)
                continue;

            const bool use_segment = segment_valid && upload_whole_segment &&
                                     command.VtxOffset == segment_vertex_offset;
            if (use_segment) {
                if (!segment_uploaded) {
                    stats.Checksum += UploadVertices(scene, segment_vertex_offset,
                                                     segment_vertex_count, working_set);
                    stats.UploadedVertexCount += segment_vertex_count;
                    ++stats.UploadCount;
                    segment_uploaded = true;
                }
                continue;
            }

            unsigned int vertex_offset = 0;
            unsigned int vertex_count = 0;
            if (!BuildDrawSlice(scene, command, working_set, vertex_offset, vertex_count))
                continue;

            stats.Checksum += UploadVertices(scene, vertex_offset, vertex_count, working_set);
            stats.UploadedVertexCount += vertex_count;
            stats.RebasedIndexCount += command.ElemCount;
            ++stats.UploadCount;
            segment_uploaded = false;
        }
        return stats;
    }

    BML_BENCHMARK_NOINLINE
    std::uint64_t BindTexture(ImTextureID texture) {
        const std::uint64_t value = (std::uint64_t)texture;
        return value * 0x9E3779B185EBCA87ULL + (value >> 7U);
    }

    PreparationStats BindEveryTexture(const Scene &scene, WorkingSet &) {
        PreparationStats stats;
        for (const ImDrawCmd &command : scene.Commands) {
            if (command.UserCallback)
                continue;
            stats.Checksum += BindTexture(command.GetTexID());
            ++stats.CallCount;
        }
        return stats;
    }

    PreparationStats BindChangedTextures(const Scene &scene, WorkingSet &) {
        PreparationStats stats;
        ImTextureID bound_texture = ImTextureID_Invalid;
        bool texture_bound = false;
        for (const ImDrawCmd &command : scene.Commands) {
            if (command.UserCallback) {
                texture_bound = false;
                continue;
            }

            const ImTextureID texture = command.GetTexID();
            if (texture_bound && texture == bound_texture)
                continue;

            stats.Checksum += BindTexture(texture);
            ++stats.CallCount;
            bound_texture = texture;
            texture_bound = true;
        }
        return stats;
    }

    Scene CreateScene(const char *name, unsigned int segment_vertex_count,
                      unsigned int segment_count, unsigned int commands_per_segment,
                      unsigned int referenced_vertices_per_segment) {
        Scene scene;
        scene.Name = name;
        scene.Vertices.resize((std::size_t)segment_vertex_count * segment_count);
        scene.Commands.reserve((std::size_t)commands_per_segment * segment_count);
        scene.Indices.reserve((std::size_t)referenced_vertices_per_segment * segment_count);

        for (std::size_t i = 0; i < scene.Vertices.size(); ++i) {
            ImDrawVert &vertex = scene.Vertices[i];
            vertex.pos = ImVec2((float)(i % 1024), (float)(i / 1024));
            vertex.uv = ImVec2((float)(i % 97) / 96.0f, (float)(i % 89) / 88.0f);
            vertex.col = 0xFF000000U | (ImU32)(i * 2654435761U);
        }

        for (unsigned int segment_index = 0; segment_index < segment_count; ++segment_index) {
            const unsigned int vertex_offset = segment_index * segment_vertex_count;
            for (unsigned int command_index = 0; command_index < commands_per_segment; ++command_index) {
                const unsigned int first = referenced_vertices_per_segment * command_index /
                                           commands_per_segment;
                const unsigned int end = referenced_vertices_per_segment * (command_index + 1) /
                                         commands_per_segment;
                ImDrawCmd command;
                command.VtxOffset = vertex_offset;
                command.IdxOffset = (unsigned int)scene.Indices.size();
                command.ElemCount = end - first;
                for (unsigned int index = first; index < end; ++index)
                    scene.Indices.push_back((ImDrawIdx)index);
                scene.Commands.push_back(command);
            }
        }
        return scene;
    }

    Scene CreateTextureScene(const char *name, unsigned int command_count,
                             unsigned int run_length, unsigned int texture_count) {
        Scene scene;
        scene.Name = name;
        if (run_length == 0 || texture_count == 0)
            return scene;
        scene.Commands.resize(command_count);
        for (unsigned int i = 0; i < command_count; ++i) {
            const std::uint64_t texture = 1U + (i / run_length) % texture_count;
            scene.Commands[i].TexRef = ImTextureRef((ImTextureID)texture);
        }
        return scene;
    }

    double MeasureBatch(PreparationFunction function, const Scene &scene, WorkingSet &working_set,
                        std::size_t iterations) {
        std::uint64_t checksum = 0;
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < iterations; ++i)
            checksum += function(scene, working_set).Checksum;
        const auto end = std::chrono::steady_clock::now();
        ResultSink.fetch_xor(checksum, std::memory_order_relaxed);
        return std::chrono::duration<double, std::nano>(end - start).count() / (double)iterations;
    }

    std::size_t Calibrate(PreparationFunction function, const Scene &scene, WorkingSet &working_set) {
        std::size_t iterations = 1;
        for (;;) {
            const auto start = std::chrono::steady_clock::now();
            MeasureBatch(function, scene, working_set, iterations);
            const auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= CalibrationTime || iterations >= (1U << 20U)) {
                const double elapsed_seconds = std::chrono::duration<double>(elapsed).count();
                const double target_seconds = std::chrono::duration<double>(SampleTime).count();
                const std::size_t scaled = elapsed_seconds > 0.0 ?
                    (std::size_t)((double)iterations * target_seconds / elapsed_seconds) : iterations;
                return std::max<std::size_t>(1, scaled);
            }
            iterations *= 2;
        }
    }

    TimingResult Summarize(std::vector<double> samples) {
        std::sort(samples.begin(), samples.end());
        TimingResult result;
        result.MinimumNanoseconds = samples.front();
        result.MedianNanoseconds = samples[samples.size() / 2];
        result.MaximumNanoseconds = samples.back();
        return result;
    }

    ComparisonResult Compare(PreparationFunction baseline, PreparationFunction candidate,
                             const Scene &scene) {
        WorkingSet baseline_working_set;
        WorkingSet candidate_working_set;
        for (int i = 0; i < 3; ++i) {
            baseline(scene, baseline_working_set);
            candidate(scene, candidate_working_set);
        }

        const std::size_t baseline_iterations = Calibrate(baseline, scene, baseline_working_set);
        const std::size_t candidate_iterations = Calibrate(candidate, scene, candidate_working_set);
        std::vector<double> baseline_samples;
        std::vector<double> candidate_samples;
        baseline_samples.reserve(SampleCount);
        candidate_samples.reserve(SampleCount);
        for (int sample = 0; sample < SampleCount; ++sample) {
            if ((sample & 1) == 0) {
                baseline_samples.push_back(MeasureBatch(
                    baseline, scene, baseline_working_set, baseline_iterations));
                candidate_samples.push_back(MeasureBatch(
                    candidate, scene, candidate_working_set, candidate_iterations));
            } else {
                candidate_samples.push_back(MeasureBatch(
                    candidate, scene, candidate_working_set, candidate_iterations));
                baseline_samples.push_back(MeasureBatch(
                    baseline, scene, baseline_working_set, baseline_iterations));
            }
        }

        ComparisonResult result;
        result.BaselineTiming = Summarize(baseline_samples);
        result.CandidateTiming = Summarize(candidate_samples);
        result.BaselineStats = baseline(scene, baseline_working_set);
        result.CandidateStats = candidate(scene, candidate_working_set);
        return result;
    }

    double GetSpeedup(const ComparisonResult &comparison) {
        return comparison.CandidateTiming.MedianNanoseconds == 0.0 ? 0.0 :
               comparison.BaselineTiming.MedianNanoseconds /
               comparison.CandidateTiming.MedianNanoseconds;
    }

    void RunGeometryScenario(const Scene &scene) {
        const ComparisonResult comparison = Compare(PrepareOld, PrepareCurrent, scene);

        std::cout << std::left << std::setw(22) << scene.Name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(12) << comparison.BaselineTiming.MedianNanoseconds / 1000.0
                  << std::setw(12) << comparison.CandidateTiming.MedianNanoseconds / 1000.0
                  << std::setw(10) << GetSpeedup(comparison)
                  << std::setw(12) << comparison.BaselineStats.UploadCount
                  << std::setw(12) << comparison.CandidateStats.UploadCount
                  << std::setw(14) << comparison.BaselineStats.UploadedVertexCount
                  << std::setw(14) << comparison.CandidateStats.UploadedVertexCount
                  << std::setw(14) << comparison.BaselineStats.RebasedIndexCount
                  << std::setw(14) << comparison.CandidateStats.RebasedIndexCount << '\n';
    }

    void RunPackingScenario(const Scene &scene, PreparationFunction generic,
                            PreparationFunction typed) {
        const ComparisonResult comparison = Compare(generic, typed, scene);
        if (comparison.BaselineStats.Checksum != comparison.CandidateStats.Checksum ||
            comparison.BaselineStats.UploadedVertexCount !=
                comparison.CandidateStats.UploadedVertexCount) {
            std::cerr << "Vertex packing validation failed for " << scene.Name << '\n';
            ValidationFailed = true;
        }
        std::cout << std::left << std::setw(22) << scene.Name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(14) << comparison.BaselineTiming.MedianNanoseconds / 1000.0
                  << std::setw(12) << comparison.CandidateTiming.MedianNanoseconds / 1000.0
                  << std::setw(10) << GetSpeedup(comparison)
                  << std::setw(14) << comparison.CandidateStats.UploadedVertexCount << '\n';
    }

    void RunTextureScenario(const Scene &scene) {
        const ComparisonResult comparison = Compare(BindEveryTexture, BindChangedTextures, scene);
        std::cout << std::left << std::setw(22) << scene.Name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(14) << comparison.BaselineTiming.MedianNanoseconds / 1000.0
                  << std::setw(12) << comparison.CandidateTiming.MedianNanoseconds / 1000.0
                  << std::setw(10) << GetSpeedup(comparison)
                  << std::setw(14) << comparison.BaselineStats.CallCount
                  << std::setw(14) << comparison.CandidateStats.CallCount << '\n';
    }
}

int main() {
    std::cout << "ImGui CK2 backend CPU microbenchmarks\n"
              << "Times exclude CK2 driver calls and GPU work; lower is better.\n\n"
              << "Geometry preparation strategy\n"
              << std::left << std::setw(22) << "Scenario"
              << std::right << std::setw(12) << "Legacy us"
              << std::setw(12) << "Current us"
              << std::setw(10) << "Speedup"
              << std::setw(12) << "Legacy up."
              << std::setw(12) << "Current up."
              << std::setw(14) << "Legacy verts"
              << std::setw(14) << "Current verts"
              << std::setw(14) << "Legacy idx"
              << std::setw(14) << "Current idx" << '\n';

    Benchmark::RunGeometryScenario(Benchmark::CreateScene("Typical menu", 12000, 1, 48, 12000));
    Benchmark::RunGeometryScenario(Benchmark::CreateScene("Many small commands", 60000, 1, 240, 60000));
    Benchmark::RunGeometryScenario(Benchmark::CreateScene("16-bit vertex limit", 65536, 1, 120, 65536));
    Benchmark::RunGeometryScenario(Benchmark::CreateScene("Dense large list", 60000, 3, 120, 60000));
    Benchmark::RunGeometryScenario(Benchmark::CreateScene("Sparse custom list", 60000, 3, 120, 4096));

    const Benchmark::Scene packing_scene = Benchmark::CreateScene(
        "60k interleaved", 60000, 1, 1, 60000);
    std::cout << "\nVertex packing kernels\n"
              << std::left << std::setw(22) << "Layout"
              << std::right << std::setw(14) << "Strided us"
              << std::setw(12) << "Typed us"
              << std::setw(10) << "Speedup"
              << std::setw(14) << "Vertices" << '\n';
    Benchmark::RunPackingScenario(
        packing_scene, Benchmark::PrepareStridedInterleaved,
        Benchmark::PrepareTypedInterleaved);
    Benchmark::Scene planar_scene = packing_scene;
    planar_scene.Name = "60k planar";
    Benchmark::RunPackingScenario(
        planar_scene, Benchmark::PrepareStridedPlanar, Benchmark::PrepareTypedPlanar);

    std::cout << "\nTexture binding dispatch (synthetic call only)\n"
              << std::left << std::setw(22) << "Pattern"
              << std::right << std::setw(14) << "Every cmd us"
              << std::setw(12) << "Cached us"
              << std::setw(10) << "Speedup"
              << std::setw(14) << "Every calls"
              << std::setw(14) << "Cached calls" << '\n';
    Benchmark::RunTextureScenario(Benchmark::CreateTextureScene("Single texture", 240, 240, 1));
    Benchmark::RunTextureScenario(Benchmark::CreateTextureScene("Grouped textures", 240, 8, 4));
    Benchmark::RunTextureScenario(Benchmark::CreateTextureScene("Alternating", 240, 1, 2));

    std::cout << "\nResult sink: " << Benchmark::ResultSink.load(std::memory_order_relaxed) << '\n';
    return Benchmark::ValidationFailed ? 1 : 0;
}
