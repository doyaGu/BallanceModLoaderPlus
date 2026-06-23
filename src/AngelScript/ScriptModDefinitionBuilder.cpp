#include "ScriptModDefinitionBuilder.h"

#include <cctype>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "BML/Defines.h"
#include "ScriptExportSignature.h"
#include "Utils/StringUtils.h"

namespace BML {

static bool StartsWithBmlTag(const std::string &name) {
    return utils::StartsWith(name, "bml.");
}

static bool LooksLikeBmlMetadata(const std::string &metadata) {
    size_t pos = 0;
    while (pos < metadata.size() && std::isspace(static_cast<unsigned char>(metadata[pos])))
        ++pos;
    return metadata.compare(pos, 4, "bml.") == 0;
}

static std::string MetadataArg(const ScriptMetadataTag &tag, const char *name) {
    auto it = tag.Args.find(name ? name : "");
    return it == tag.Args.end() ? std::string() : it->second;
}

static std::string RequireMetadataArg(const ScriptMetadataTag &tag,
                                      const char *name,
                                      const std::string &metadata,
                                      std::vector<std::string> &diagnostics) {
    const std::string value = MetadataArg(tag, name);
    if (value.empty()) {
        diagnostics.push_back("metadata '" + metadata + "' is missing required argument '" +
                              (name ? name : "") + "'.");
    }
    return value;
}

struct ScriptMetadataRecord {
    CKAS_METADATA_TARGET Target = CKAS_METADATA_TYPE;
    std::string Name;
    std::string Namespace;
    std::string Declaration;
    std::string ParentTypeName;
    std::string ParentTypeNamespace;
    std::string Metadata;
};

struct ScriptMetadataCollection {
    std::vector<ScriptMetadataRecord> Records;
};

struct ParsedScriptMetadataRecord {
    const ScriptMetadataRecord *Record = nullptr;
    ScriptMetadataTag Tag;
};

static CKAS_STATUS CollectScriptMetadata(const CKAngelScriptMetadataEntry *entry,
                                         CKDWORD,
                                         const char *metadata,
                                         void *userData) {
    auto *collection = static_cast<ScriptMetadataCollection *>(userData);
    if (!entry || !collection)
        return CKAS_INVALIDARGUMENT;

    ScriptMetadataRecord record;
    record.Target = entry->Target;
    record.Name = entry->Name ? entry->Name : "";
    record.Namespace = entry->Namespace ? entry->Namespace : "";
    record.Declaration = entry->Declaration ? entry->Declaration : "";
    record.ParentTypeName = entry->ParentTypeName ? entry->ParentTypeName : "";
    record.ParentTypeNamespace = entry->ParentTypeNamespace ? entry->ParentTypeNamespace : "";
    record.Metadata = metadata ? metadata : "";
    collection->Records.push_back(record);
    return CKAS_OK;
}

bool ScriptModDefinitionBuilder::Build(CKContext *context,
                                       const ScriptModEntry &entry,
                                       ScriptModRuntime &runtime,
                                       ScriptModDefinition &definition,
                                       ScriptDiagnostic &diagnostic) const {
    if (!runtime.IsModuleLoaded() &&
        !runtime.LoadModule(context, utils::Utf16ToUtf8(entry.EntryPath), diagnostic)) {
        return false;
    }

    ScriptMetadataCollection collection;
    if (!runtime.EnumerateMetadata(context, CollectScriptMetadata, &collection, diagnostic))
        return false;

    std::vector<std::string> diagnostics;
    const ScriptMetadataRecord *mainType = nullptr;
    std::vector<ParsedScriptMetadataRecord> bmlMetadata;
    std::unordered_set<std::string> exportKeys;
    ScriptModDefinition reflected;
    reflected.Entry = entry.EntryFilename;
    reflected.Enabled = true;

    for (const ScriptMetadataRecord &record : collection.Records) {
        if (!LooksLikeBmlMetadata(record.Metadata))
            continue;

        ScriptMetadataTag tag;
        std::string parseDiagnostic;
        if (!ParseScriptMetadataTag(record.Metadata, tag, parseDiagnostic)) {
            diagnostics.push_back("metadata '" + record.Metadata + "': " + parseDiagnostic);
            continue;
        }
        if (!StartsWithBmlTag(tag.Name))
            continue;
        bmlMetadata.push_back(ParsedScriptMetadataRecord{&record, tag});
    }

    for (const ParsedScriptMetadataRecord &metadata : bmlMetadata) {
        const ScriptMetadataRecord &record = *metadata.Record;
        const ScriptMetadataTag &tag = metadata.Tag;
        if (tag.Name != "bml.mod")
            continue;

        if (record.Target != CKAS_METADATA_TYPE) {
            diagnostics.push_back("bml.mod metadata must be attached to a class.");
            continue;
        }
        if (mainType) {
            diagnostics.push_back("multiple classes declare bml.mod metadata.");
            continue;
        }
        mainType = &record;
        reflected.ClassName = record.Name;
        reflected.ClassNamespace = record.Namespace;
        reflected.Id = RequireMetadataArg(tag, "id", record.Metadata, diagnostics);
        reflected.Name = RequireMetadataArg(tag, "name", record.Metadata, diagnostics);
        reflected.Version = RequireMetadataArg(tag, "version", record.Metadata, diagnostics);
        reflected.Author = MetadataArg(tag, "author");
        reflected.Description = MetadataArg(tag, "description");
        reflected.ReloadPolicy = MetadataArg(tag, "reload");
        if (!reflected.ReloadPolicy.empty() &&
            ParseScriptModReloadPolicy(reflected.ReloadPolicy) == ScriptModReloadPolicy::Default) {
            diagnostics.push_back("bml.mod reload must be 'auto' or 'manual'.");
        }
        const std::string bmlVersion = MetadataArg(tag, "bml");
        if (!bmlVersion.empty()) {
            reflected.Metadata["bml"] = bmlVersion;
            reflected.MinBmlVersion = ParseBmlVersion(bmlVersion);
        }
    }

    for (const ParsedScriptMetadataRecord &metadata : bmlMetadata) {
        const ScriptMetadataRecord &record = *metadata.Record;
        const ScriptMetadataTag &tag = metadata.Tag;

        if (tag.Name == "bml.mod")
            continue;

        if (tag.Name == "bml.require" || tag.Name == "bml.optional") {
            ScriptModDependency dependency;
            dependency.Id = RequireMetadataArg(tag, "id", record.Metadata, diagnostics);
            dependency.Optional = tag.Name == "bml.optional";
            const std::string version = MetadataArg(tag, "version");
            if (!version.empty())
                dependency.MinVersion = ParseBmlVersion(version);
            reflected.Dependencies.push_back(dependency);
            continue;
        }

        if (tag.Name == "bml.export") {
            if (record.Target != CKAS_METADATA_TYPE_METHOD) {
                diagnostics.push_back("bml.export metadata must be attached to a class method.");
                continue;
            }
            if (!mainType) {
                diagnostics.push_back("bml.export metadata appeared before bml.mod metadata.");
                continue;
            }
            if (record.ParentTypeName != mainType->Name ||
                record.ParentTypeNamespace != mainType->Namespace) {
                diagnostics.push_back("bml.export method '" + record.Name + "' is not on the bml.mod class.");
                continue;
            }

            ScriptModExportDefinition exportInfo;
            exportInfo.Method = record.Name;
            exportInfo.Name = MetadataArg(tag, "name");
            if (exportInfo.Name.empty())
                exportInfo.Name = record.Name;
            exportInfo.Signature = ScriptExportSignature::Canonicalize(record.Declaration,
                                                                        exportInfo.Name);
            const int signatureStatus = InteropSignature::Validate(exportInfo.Signature, exportInfo.Name);
            if (signatureStatus != BML_OK) {
                diagnostics.push_back("bml.export name '" + exportInfo.Name +
                                      "' has unsupported interop signature '" +
                                      exportInfo.Signature + "'.");
                continue;
            }
            const std::string exportKey = exportInfo.Name + "\n" + exportInfo.Signature;
            if (!exportKeys.insert(exportKey).second) {
                diagnostics.push_back("duplicate bml.export name '" + exportInfo.Name +
                                      "' with signature '" + exportInfo.Signature + "'.");
                continue;
            }
            reflected.Exports.push_back(exportInfo);
            continue;
        }

        diagnostics.push_back("unknown BML metadata tag '" + tag.Name + "'.");
    }

    if (!mainType)
        diagnostics.push_back("missing required class metadata 'bml.mod'.");
    if (reflected.Id.empty())
        diagnostics.push_back("bml.mod is missing required id.");
    if (reflected.Name.empty())
        diagnostics.push_back("bml.mod is missing required name.");
    if (reflected.Version.empty())
        diagnostics.push_back("bml.mod is missing required version.");

    if (!diagnostics.empty()) {
        std::ostringstream message;
        message << "Metadata invalid:";
        for (const std::string &item : diagnostics)
            message << " " << item;
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Metadata, message.str());
        diagnostic.EntryPath = utils::Utf16ToUtf8(entry.EntryPath);
        return false;
    }

    definition = reflected;
    return true;
}

} // namespace BML
