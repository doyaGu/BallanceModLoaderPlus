[bml.mod id="example.game.state"
         name="Game State Example"
         version="1.0.0"
         author="BML+"
         description="Reads level state and the active ball"
         bml="0.3.13"]
class GameStateExample {
    void OnLoad(const BML::ModContext &in ctx) {
        ctx.LogInfo("Game state example loaded; enter a level");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event != BML::GAME_EVENT_START_LEVEL)
            return;

        BML::Runtime::State runtime = BML::Runtime::GetState();
        if (!runtime.InLevel) {
            ctx.LogWarn("Start-level event arrived without an active level");
            return;
        }

        BML::Gameplay::LevelState level;
        int status = BML::Gameplay::ReadLevel(level);
        if (status != BML::ERROR_OK) {
            ctx.LogWarn("Could not read level state; status=" + status);
            return;
        }

        CKObject@ ball = level.BorrowActiveBall();
        if (ball is null) {
            ctx.LogWarn("Level " + level.Id + " has no active ball");
            return;
        }

        string name = BML::CK::GetName(ball);
        ctx.LogInfo("Level " + level.Id + " active ball: " + name);
        BML::UI::AddMessage("Active ball: " + name);
    }
}
