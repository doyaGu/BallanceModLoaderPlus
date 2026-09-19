#ifndef BML_BUILTIN_MOD_MENU_H
#define BML_BUILTIN_MOD_MENU_H

#include <memory>

class ModContext;

class ModMenu final {
public:
    explicit ModMenu(ModContext &context);
    ~ModMenu();

    ModMenu(const ModMenu &) = delete;
    ModMenu &operator=(const ModMenu &) = delete;

    bool Open();
    bool Close();
    bool CloseForShutdown();
    void OnProcess();

private:
    struct State;
    std::unique_ptr<State> m_State;
};

#endif // BML_BUILTIN_MOD_MENU_H
