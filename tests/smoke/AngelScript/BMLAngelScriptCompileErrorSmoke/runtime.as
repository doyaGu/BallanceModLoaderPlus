[bml.mod id="bml.compile.error.smoke" name="BML AngelScript Compile Error Smoke" version="0.0.0" author="BML" bml="0.3.0" description="Negative smoke test for script mod compile diagnostics."]
class BMLCompileErrorSmokeMod {
  void OnLoad(const BML::ModContext &in ctx) {
    ctx.BorrowLogger().Info("this script is intentionally invalid")
  }
}
