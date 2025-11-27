/**
 * @file bml_diagnostics.h
 * @brief Compatibility header - redirects to bml_errors.h
 * 
 * This header is deprecated. All error and diagnostic functionality
 * has been consolidated into bml_errors.h.
 * 
 * @deprecated Use bml_errors.h instead
 * @since 0.4.0
 */

#ifndef BML_DIAGNOSTICS_H
#define BML_DIAGNOSTICS_H

/* All functionality has been merged into bml_errors.h */
#include "bml_errors.h"

#ifdef __cplusplus
/* Provide namespace alias for compatibility */
namespace bml_diagnostics = bml;
#endif

#endif /* BML_DIAGNOSTICS_H */
