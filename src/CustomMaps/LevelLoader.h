#ifndef BML_CUSTOMMAPS_LEVELLOADER_H
#define BML_CUSTOMMAPS_LEVELLOADER_H

#include <optional>
#include <string>
#include <string_view>

#include "BML/Behavior.hpp"

class CKDataArray;

namespace CustomMap {

class LevelLoader {
public:
    class Transaction {
    public:
        Transaction() = default;
        Transaction(Transaction &&) noexcept = default;
        Transaction &operator=(Transaction &&) noexcept = default;

        Transaction(const Transaction &) = delete;
        Transaction &operator=(const Transaction &) = delete;

        [[nodiscard]] explicit operator bool() const noexcept;
        [[nodiscard]] BML::Behavior::Result<void> Stage(
            std::string_view mapFile, int level,
            CKDataArray *currentLevel) const;
        [[nodiscard]] BML::Behavior::Result<void> Rollback(
            CKDataArray *currentLevel) const;
        [[nodiscard]] BML::Behavior::Result<void> ClearRoute() const;

    private:
        struct Snapshot {
            std::string MapFile;
            int CurrentLevel = 0;
            int LevelRow = 0;
            bool LoadCustom = false;
        };

        Transaction(BML::Behavior::Plan::Instance instance,
                    BML::Behavior::Edit::Port mapFile,
                    BML::Behavior::Edit::Port levelRow,
                    BML::Behavior::Edit::Port loadCustom,
                    Snapshot previous);

        BML::Behavior::Plan::Instance m_Instance;
        BML::Behavior::Edit::Port m_MapFile;
        BML::Behavior::Edit::Port m_LevelRow;
        BML::Behavior::Edit::Port m_LoadCustom;
        Snapshot m_Previous;

        friend class LevelLoader;
    };

    [[nodiscard]] BML::Behavior::Result<void> Install(
        BML::Behavior::Session &behavior);
    [[nodiscard]] BML::Behavior::Result<void> Refresh();
    [[nodiscard]] BML::Behavior::Result<Transaction> Begin(
        CKDataArray *currentLevel) const;
    [[nodiscard]] BML::Behavior::Result<BML::Behavior::CloseState> Close();

    void Invalidate() noexcept;
    [[nodiscard]] bool IsReady() const noexcept;
    [[nodiscard]] bool IsRetiring() const noexcept;

private:
    template <class T>
    [[nodiscard]] static BML::Behavior::Result<T> Read(
        const BML::Behavior::Plan::Instance &instance,
        const BML::Behavior::Edit::Port &port, std::string_view name);

    [[nodiscard]] static BML::Behavior::Status AddContext(
        BML::Behavior::Status status, std::string message);
    [[nodiscard]] static BML::Behavior::Result<void> AddContext(
        BML::Behavior::Result<void> result, std::string message);

    template <class T = void>
    [[nodiscard]] static BML::Behavior::Result<T> Failure(
        int code, BML::Behavior::Error error, std::string message);

    static void RememberFirstFailure(
        BML::Behavior::Result<void> &result,
        BML::Behavior::Result<void> candidate);
    void ClearPorts();

    BML::Behavior::Plan m_Plan;
    std::optional<BML::Behavior::Plan::Instance> m_Instance;
    BML::Behavior::Edit::Port m_MapFile;
    BML::Behavior::Edit::Port m_LevelRow;
    BML::Behavior::Edit::Port m_LoadCustom;
    bool m_Retiring = false;
    bool m_ClosePending = false;
};

} // namespace CustomMap

#endif // BML_CUSTOMMAPS_LEVELLOADER_H
