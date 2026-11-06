#include "Loader/ModInvocationGate.h"

namespace BML {

// One definition per module, including the TLS initialization routines. Keeping
// these out of the header also works with hidden visibility on Mach-O.
thread_local std::vector<ModInvocationGate::CallEntry> ModInvocationGate::s_CallEntries;
thread_local std::vector<ModInvocationGate::CallEntry> ModInvocationGate::s_MutationEntries;

} // namespace BML
