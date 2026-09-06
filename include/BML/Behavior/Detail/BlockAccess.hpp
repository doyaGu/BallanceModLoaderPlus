#ifndef BML_BEHAVIOR_DETAIL_BLOCKACCESS_HPP
#define BML_BEHAVIOR_DETAIL_BLOCKACCESS_HPP

namespace BML::Behavior::Detail {

// The named Block adapters keep their mapping private. Public Make functions
// and the loader's native compatibility layer share it through this Detail
// access point without making a builder protocol part of Options.
class BlockAccess final {
public:
    template <class Options, class Definition>
    static void Configure(const Options &options, Definition &definition) {
        options.Configure(definition);
    }
};

} // namespace BML::Behavior::Detail

#endif // BML_BEHAVIOR_DETAIL_BLOCKACCESS_HPP
