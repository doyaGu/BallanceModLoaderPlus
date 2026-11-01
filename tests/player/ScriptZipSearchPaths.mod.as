[bml.mod id="bml.zip.paths" name="ZIP Search Paths" version="1.0.0" author="BML" bml="0.3.14" reload="manual"]
class ZipSearchPaths {
    bool RejectReload = false;
    void OnLoad(const BML::ModContext &in ctx) {
        if (ctx.ReloadPhase == BML::RELOAD_LOAD) {
            ctx.ExecuteCommand("zip_paths_phase candidate");
            if (RejectReload)
                throw("intentional ZIP reload rejection");
        } else if (ctx.ReloadPhase == BML::RELOAD_ROLLBACK) {
            ctx.ExecuteCommand("zip_paths_phase rollback");
        }
        ctx.LogInfo("Script ZIP search paths loaded");
    }
}
