#ifndef BML_IVP_ACTIVE_VALUE_H
#define BML_IVP_ACTIVE_VALUE_H

#include "BML/IVP/Set.h"

#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cstring>

class IVP_Active_Value_Hash;
class IVP_Environment;
class IVP_U_Active_Float;
class IVP_U_Active_Float_Delayed;
class IVP_U_Active_Int;
class IVP_U_Active_Int_Delayed;
class IVP_U_Active_Terminal_Double;
class IVP_U_Active_Terminal_Int;
class IVP_U_Active_Value;

inline constexpr const char *IVP_ACTIVE_FLOAT_CURRENT_TIME_NAME =
    "current_time";

class IVP_U_Active_Float_Listener {
public:
    virtual void active_float_changed(IVP_U_Active_Float *callingValue) = 0;
};

class IVP_U_Active_Int_Listener {
public:
    virtual void active_int_changed(IVP_U_Active_Int *callingValue) = 0;
};

class IVP_U_Active_Int_Delayed {
public:
    virtual void update_int() = 0;
};

class IVP_U_Active_Float_Delayed {
public:
    virtual void update_float() = 0;
};

// The destructor is the first slot in the retail 15-slot manager table.
class IVP_U_Active_Value_Manager {
    friend struct BML_IvpActiveValueManagerLayoutCheck;

public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    explicit IVP_U_Active_Value_Manager(IVP_BOOL deleteOnEnvironmentDelete) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerConstruct,
            this, deleteOnEnvironmentDelete);
    }
    virtual ~IVP_U_Active_Value_Manager() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerDestruct, this);
    }
    virtual void environment_will_be_deleted(IVP_Environment *environment) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerEnvironmentDelete,
            this, environment);
    }
    virtual void insert_active_float(IVP_U_Active_Float *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerInsertFloat,
            this, value);
    }
    virtual void remove_active_float(IVP_U_Active_Float *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerRemoveFloat,
            this, value);
    }
    virtual void insert_active_int(IVP_U_Active_Int *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerInsertInt,
            this, value);
    }
    virtual void remove_active_int(IVP_U_Active_Int *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerRemoveInt,
            this, value);
    }
    virtual void delay_active_float(IVP_U_Active_Float_Delayed *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerDelayFloat,
            this, value);
    }
    virtual void delay_active_int(IVP_U_Active_Int_Delayed *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerDelayInt,
            this, value);
    }
    virtual void update_delayed_active_values() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerUpdateDelayed, this);
    }
    virtual void init_active_values_generic() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerInitialize, this);
    }
    virtual void refresh_psi_active_values(IVP_Environment *environment) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActiveValueManagerRefreshPsi,
            this, environment);
    }
    virtual IVP_U_Active_Float *install_active_float(
        const char *name, IVP_DOUBLE value) {
        return BML::IVP::ABI::InvokeThis<IVP_U_Active_Float *>(
            BML::IVP::ABI::Address::ActiveValueManagerInstallFloat,
            this, name, value);
    }
    virtual IVP_U_Active_Terminal_Double *create_active_float(
        const char *name, IVP_DOUBLE value) {
        return BML::IVP::ABI::InvokeThis<IVP_U_Active_Terminal_Double *>(
            BML::IVP::ABI::Address::ActiveValueManagerCreateFloat,
            this, name, value);
    }
    IVP_U_Active_Float *get_active_float_by_name(const char *name);
    virtual IVP_U_Active_Int *install_active_int(
        const char *name, int value) {
        return BML::IVP::ABI::InvokeThis<IVP_U_Active_Int *>(
            BML::IVP::ABI::Address::ActiveValueManagerInstallInt,
            this, name, value);
    }
    virtual IVP_U_Active_Terminal_Int *create_active_int(
        const char *name, int value) {
        return BML::IVP::ABI::InvokeThis<IVP_U_Active_Terminal_Int *>(
            BML::IVP::ABI::Address::ActiveValueManagerCreateInt,
            this, name, value);
    }
    IVP_U_Active_Int *get_active_int_by_name(const char *name);

private:
    IVP_BOOL delete_on_env_delete;
    IVP_Active_Value_Hash *floats_name_hash;
    IVP_Active_Value_Hash *ints_name_hash;
    IVP_U_Vector_Base delayed_active_floats;
    IVP_U_Vector_Base delayed_active_ints;
    IVP_U_Active_Terminal_Double *mod_current_time;
    IVP_U_Active_Value *search_active_value;
};

class IVP_U_Active_Value {
private:
    friend class IVP_U_Active_Value_Manager;
    friend struct BML_IvpActiveValueLayoutCheck;

    char *name;

protected:
    struct Retail_Construction_Tag {};

    explicit IVP_U_Active_Value(Retail_Construction_Tag) noexcept {}

    void initialize_reconstructed(const char *valueName) {
        name = valueName ? BML::IVP::ABI::Invoke<char *>(
                               BML::IVP::ABI::Address::DuplicateString,
                               valueName)
                         : nullptr;
        reference_count = 0;
    }

    int reference_count;

public:
    explicit IVP_U_Active_Value(const char *valueName) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActiveValueConstruct, this,
            [this, valueName] { initialize_reconstructed(valueName); },
            valueName);
    }
    void add_reference() { ++reference_count; }
    void remove_reference() {
        if (--reference_count == 0)
            delete this;
    }
    virtual ~IVP_U_Active_Value() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActiveValueDestruct, this,
            [this] {
                if (name) {
                    BML::IVP::ABI::Invoke<void>(
                        BML::IVP::ABI::Address::Free, name);
                    name = nullptr;
                }
            });
    }

    const char *get_name() { return name; }
    const char *get_name() const { return name; }

    BML_IVP_RETAIL_ALLOCATED_OBJECT;
};

// Ballance retains object_to_index() and compare(), while the three short
// typed set operations are inlined at every call site.  Keep the source
// inheritance and vtable shape; IVP_VHash supplies the retail table engine.
class IVP_Active_Value_Hash : protected IVP_VHash {
protected:
    IVP_BOOL compare(void *left, void *right) const override {
        const auto *leftValue = static_cast<const IVP_U_Active_Value *>(left);
        const auto *rightValue = static_cast<const IVP_U_Active_Value *>(right);
        return std::strcmp(leftValue->get_name(), rightValue->get_name()) == 0
            ? IVP_TRUE
            : IVP_FALSE;
    }

    int object_to_index(IVP_U_Active_Value *value) {
        const char *name = value->get_name();
        return hash_index(name, static_cast<int>(std::strlen(name)));
    }

public:
    explicit IVP_Active_Value_Hash(int initialSize)
        : IVP_VHash(initialSize) {}

    ~IVP_Active_Value_Hash() override {
        for (int index = len() - 1; index >= 0; --index) {
            auto *value = static_cast<IVP_U_Active_Value *>(element_at(index));
            if (value)
                value->remove_reference();
        }
    }

    void add_active_value(IVP_U_Active_Value *value) {
        add_elem(value, object_to_index(value));
        value->add_reference();
    }

    IVP_U_Active_Value *remove_active_value(IVP_U_Active_Value *value) {
        auto *removed = static_cast<IVP_U_Active_Value *>(
            remove_elem(value, static_cast<unsigned int>(object_to_index(value))));
        removed->remove_reference();
        return removed;
    }

    IVP_U_Active_Value *find_active_value(IVP_U_Active_Value *value) {
        return static_cast<IVP_U_Active_Value *>(
            find_elem(value, static_cast<unsigned int>(object_to_index(value))));
    }
};

class IVP_U_Active_Float : public IVP_U_Active_Value {
private:
    // The retained complete constructor owns this member's construction.
    union {
        IVP_U_Vector<IVP_U_Active_Float_Listener> derived_mods;
    };

protected:
    struct Retail_Construction_Tag {};

    explicit IVP_U_Active_Float(Retail_Construction_Tag) noexcept
        : IVP_U_Active_Value(IVP_U_Active_Value::Retail_Construction_Tag{}) {}

    void initialize_reconstructed(const char *valueName) {
        IVP_U_Active_Value::initialize_reconstructed(valueName);
        ::new (static_cast<void *>(&derived_mods))
            IVP_U_Vector<IVP_U_Active_Float_Listener>();
        l_mod_manager = nullptr;
        last_update = 0;
        double_value = 0.0;
    }

    IVP_U_Active_Value_Manager *l_mod_manager;

public:
    int last_update;
    IVP_DOUBLE double_value;

    explicit IVP_U_Active_Float(const char *valueName)
        : IVP_U_Active_Value(IVP_U_Active_Value::Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActiveFloatConstruct, this,
            [this, valueName] { initialize_reconstructed(valueName); },
            valueName);
    }
    virtual ~IVP_U_Active_Float() {
        derived_mods.~IVP_U_Vector<IVP_U_Active_Float_Listener>();
    }
    virtual int print() = 0;

    inline static int change_meter = -1;

    IVP_DOUBLE give_double_value() { return double_value; }
    IVP_DOUBLE give_double_value() const { return double_value; }
    IVP_FLOAT get_float_value() {
        return static_cast<IVP_FLOAT>(double_value);
    }
    IVP_FLOAT get_float_value() const {
        return static_cast<IVP_FLOAT>(double_value);
    }
    void update_derived() {
        for (int index = derived_mods.len() - 1; index >= 0; --index)
            derived_mods.element_at(index)->active_float_changed(this);
    }
    void add_dependency(IVP_U_Active_Float_Listener *listener) {
        derived_mods.add(listener);
        add_reference();
    }
    void remove_dependency(IVP_U_Active_Float_Listener *listener) {
        derived_mods.remove(listener);
        remove_reference();
    }
};

class IVP_U_Active_Int : public IVP_U_Active_Value {
private:
    // The retained complete constructor owns this member's construction.
    union {
        IVP_U_Vector<IVP_U_Active_Int_Listener> derived_mods;
    };

protected:
    struct Retail_Construction_Tag {};

    explicit IVP_U_Active_Int(Retail_Construction_Tag) noexcept
        : IVP_U_Active_Value(IVP_U_Active_Value::Retail_Construction_Tag{}) {}

    void initialize_reconstructed(const char *valueName) {
        IVP_U_Active_Value::initialize_reconstructed(valueName);
        ::new (static_cast<void *>(&derived_mods))
            IVP_U_Vector<IVP_U_Active_Int_Listener>();
        l_mod_manager = nullptr;
        last_update = 0;
        int_value = 0;
    }

    IVP_U_Active_Value_Manager *l_mod_manager;

public:
    int last_update;
    int int_value;

    explicit IVP_U_Active_Int(const char *valueName)
        : IVP_U_Active_Value(IVP_U_Active_Value::Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActiveIntConstruct, this,
            [this, valueName] { initialize_reconstructed(valueName); },
            valueName);
    }
    virtual ~IVP_U_Active_Int() {
        derived_mods.~IVP_U_Vector<IVP_U_Active_Int_Listener>();
    }
    virtual int print() = 0;

    int give_int_value() { return int_value; }
    int give_int_value() const { return int_value; }
    void update_derived() {
        for (int index = derived_mods.len() - 1; index >= 0; --index)
            derived_mods.element_at(index)->active_int_changed(this);
    }
    void add_dependency(IVP_U_Active_Int_Listener *listener) {
        derived_mods.add(listener);
        add_reference();
    }
    void remove_dependency(IVP_U_Active_Int_Listener *listener) {
        derived_mods.remove(listener);
        remove_reference();
    }
};

class IVP_U_Active_Terminal_Double : public IVP_U_Active_Float,
                                     public IVP_U_Active_Float_Delayed {
    friend struct BML_IvpActiveTerminalDoubleLayoutCheck;

public:
    IVP_U_Active_Terminal_Double(
        const char *valueName, IVP_DOUBLE value)
        : IVP_U_Active_Float(IVP_U_Active_Float::Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActiveTerminalDoubleConstruct, this,
            [this, valueName, value] {
                initialize_reconstructed(valueName);
                double_value = value;
                old_value = value;
            },
            valueName, value);
    }

    void update_float() override {
        // The retained entry is the secondary-base implementation used by
        // IVP_U_Active_Float_Delayed's vtable. Its ECX points at the delayed
        // subobject (+0x28), not at the complete terminal object.
        auto *delayedSelf =
            static_cast<IVP_U_Active_Float_Delayed *>(this);
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActiveTerminalDoubleUpdate, delayedSelf,
            [this] {
                if (double_value == old_value) return;
                old_value = double_value;
                update_derived();
            });
    }
    virtual void set_double(
        IVP_DOUBLE newValue, IVP_BOOL delayed = IVP_FALSE) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActiveTerminalDoubleSet,
            this, [this, newValue, delayed] {
                double_value = newValue;
                ++change_meter;
                if (delayed != IVP_FALSE && l_mod_manager)
                    l_mod_manager->delay_active_float(this);
                else
                    update_float();
            }, newValue, delayed);
    }
    int print() override { return 0; }

private:
    IVP_DOUBLE old_value;
};

class IVP_U_Active_Terminal_Int : public IVP_U_Active_Int,
                                  public IVP_U_Active_Int_Delayed {
    friend struct BML_IvpActiveTerminalIntLayoutCheck;

public:
    IVP_U_Active_Terminal_Int(const char *valueName, int value)
        : IVP_U_Active_Int(IVP_U_Active_Int::Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActiveTerminalIntConstruct, this,
            [this, valueName, value] {
                initialize_reconstructed(valueName);
                int_value = value;
                old_value = value;
            },
            valueName, value);
    }

    void update_int() override {
        // As above, the retail body receives the Int Delayed secondary base
        // (+0x20). Passing the complete object would make its -4/+4 accesses
        // read last_update and the secondary vptr instead of current/old.
        auto *delayedSelf =
            static_cast<IVP_U_Active_Int_Delayed *>(this);
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActiveTerminalIntUpdate, delayedSelf,
            [this] {
                if (int_value == old_value) return;
                old_value = int_value;
                update_derived();
            });
    }
    virtual void set_int(int newValue, IVP_BOOL delayed = IVP_FALSE) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActiveTerminalIntSet,
            this, [this, newValue, delayed] {
                int_value = newValue;
                ++IVP_U_Active_Float::change_meter;
                if (delayed != IVP_FALSE && l_mod_manager)
                    l_mod_manager->delay_active_int(this);
                else
                    update_int();
            }, newValue, delayed);
    }
    int print() override { return 0; }

private:
    int old_value;
};

// The retail image strips these public expression-node bodies because
// physics_RT does not instantiate them.  They are reconstructed individually
// from the nearby implementation on top of the verified Ballance active-value
// base layouts, rather than borrowing another revision's manager or allocator.
class IVP_U_Active_Sine : public IVP_U_Active_Float,
                          public IVP_U_Active_Float_Listener {
public:
    IVP_U_Active_Sine(const char *name, IVP_U_Active_Float *timeValue,
                      IVP_DOUBLE frequency, IVP_DOUBLE amplitudeValue,
                      IVP_DOUBLE nullLevel, IVP_DOUBLE timeShift)
        : IVP_U_Active_Float(name), time_mod(timeValue),
          frequence(frequency), amplitude(amplitudeValue),
          null_level(nullLevel), time_shift(timeShift) {
        time_mod->add_dependency(this);
        active_float_changed(this);
    }

    ~IVP_U_Active_Sine() override { time_mod->remove_dependency(this); }

    void active_float_changed(IVP_U_Active_Float *) override {
        if (last_update == change_meter)
            return;
        last_update = change_meter;
        const IVP_DOUBLE value = IVP_Inline_Math::sind(
            time_mod->give_double_value() * frequence + time_shift) *
            amplitude + null_level;
        if (value != double_value) {
            double_value = value;
            update_derived();
        }
    }

    int print() override {
        std::printf("Sine[F %g, A %g, N %g, ts %g](",
                    frequence, amplitude, null_level, time_shift);
        time_mod->print();
        std::printf(")");
        return 0;
    }

private:
    IVP_U_Active_Float *time_mod;
    IVP_DOUBLE frequence;
    IVP_DOUBLE amplitude;
    IVP_DOUBLE null_level;
    IVP_DOUBLE time_shift;
};

class IVP_U_Active_Square : public IVP_U_Active_Float,
                            public IVP_U_Active_Float_Listener {
public:
    IVP_U_Active_Square(const char *name, IVP_U_Active_Float *timeValue,
                        IVP_DOUBLE frequency, IVP_DOUBLE lowValue,
                        IVP_DOUBLE highValue)
        : IVP_U_Active_Float(name), time_mod(timeValue),
          frequence(frequency), low_val(lowValue), high_val(highValue) {
        time_mod->add_dependency(this);
    }

    ~IVP_U_Active_Square() override { time_mod->remove_dependency(this); }

    void active_float_changed(IVP_U_Active_Float *) override {
        if (last_update == change_meter)
            return;
        last_update = change_meter;
        const int phase = static_cast<int>(
            time_mod->give_double_value() * frequence) & 1;
        const IVP_DOUBLE value = phase ? high_val : low_val;
        if (value != double_value) {
            double_value = value;
            update_derived();
        }
    }

    int print() override {
        std::printf("Square[F %g, L %g, H %g](",
                    frequence, low_val, high_val);
        time_mod->print();
        std::printf(")");
        return 0;
    }

private:
    IVP_U_Active_Float *time_mod;
    IVP_DOUBLE frequence;
    IVP_DOUBLE low_val;
    IVP_DOUBLE high_val;
};

class IVP_U_Active_Pulse : public IVP_U_Active_Float,
                           public IVP_U_Active_Float_Listener {
public:
    IVP_U_Active_Pulse(const char *name, IVP_U_Active_Float *timeValue,
                       IVP_DOUBLE frequency, int activeParts,
                       int totalParts, IVP_DOUBLE lowValue,
                       IVP_DOUBLE highValue)
        : IVP_U_Active_Float(name), time_mod(timeValue),
          frequence(frequency), low_val(lowValue), high_val(highValue),
          m1(activeParts), m2(totalParts) {
        time_mod->add_dependency(this);
        active_float_changed(this);
    }

    ~IVP_U_Active_Pulse() override { time_mod->remove_dependency(this); }

    void active_float_changed(IVP_U_Active_Float *) override {
        if (last_update == change_meter)
            return;
        last_update = change_meter;
        const int phase = static_cast<int>(
            time_mod->give_double_value() * frequence * m2) % m2;
        const IVP_DOUBLE value = phase < m1 ? high_val : low_val;
        if (value != double_value) {
            double_value = value;
            update_derived();
        }
    }

    int print() override {
        std::printf("Pulse[F %g, L %g, H %g, %d:%d](",
                    frequence, low_val, high_val, m1, m2);
        time_mod->print();
        std::printf(")");
        return 0;
    }

private:
    IVP_U_Active_Float *time_mod;
    IVP_DOUBLE frequence;
    IVP_DOUBLE low_val;
    IVP_DOUBLE high_val;
    int m1;
    int m2;
};

class IVP_U_Active_Add : public IVP_U_Active_Float,
                         public IVP_U_Active_Float_Listener {
public:
    IVP_U_Active_Add(const char *name, IVP_U_Active_Float *left,
                     IVP_U_Active_Float *right)
        : IVP_U_Active_Float(name), mod0(left), mod1(right) {
        mod0->add_dependency(this);
        mod1->add_dependency(this);
        active_float_changed(this);
    }

    ~IVP_U_Active_Add() override {
        mod0->remove_dependency(this);
        mod1->remove_dependency(this);
    }

    void active_float_changed(IVP_U_Active_Float *) override {
        if (last_update == change_meter)
            return;
        last_update = change_meter;
        const IVP_DOUBLE value =
            mod0->give_double_value() + mod1->give_double_value();
        if (value != double_value) {
            double_value = value;
            update_derived();
        }
    }

    int print() override {
        mod0->print();
        std::printf(" + ");
        mod1->print();
        return 0;
    }

private:
    IVP_U_Active_Float *mod0;
    IVP_U_Active_Float *mod1;
};

class IVP_U_Active_Sub : public IVP_U_Active_Float,
                         public IVP_U_Active_Float_Listener {
public:
    IVP_U_Active_Sub(const char *name, IVP_U_Active_Float *left,
                     IVP_U_Active_Float *right)
        : IVP_U_Active_Float(name), mod0(left), mod1(right) {
        mod0->add_dependency(this);
        mod1->add_dependency(this);
        active_float_changed(this);
    }

    ~IVP_U_Active_Sub() override {
        mod0->remove_dependency(this);
        mod1->remove_dependency(this);
    }

    void active_float_changed(IVP_U_Active_Float *) override {
        if (last_update == change_meter)
            return;
        last_update = change_meter;
        const IVP_DOUBLE value =
            mod0->give_double_value() - mod1->give_double_value();
        if (value != double_value) {
            double_value = value;
            update_derived();
        }
    }

    int print() override {
        mod0->print();
        std::printf(" - ");
        mod1->print();
        return 0;
    }

private:
    IVP_U_Active_Float *mod0;
    IVP_U_Active_Float *mod1;
};

class IVP_U_Active_Add_Multiple : public IVP_U_Active_Float,
                                  public IVP_U_Active_Float_Listener {
public:
    IVP_U_Active_Add_Multiple(const char *name, IVP_U_Active_Float *left,
                              IVP_U_Active_Float *right,
                              IVP_DOUBLE multiplier)
        : IVP_U_Active_Float(name), mod0(left), mod1(right),
          factor(multiplier) {
        mod0->add_dependency(this);
        mod1->add_dependency(this);
        active_float_changed(this);
    }

    ~IVP_U_Active_Add_Multiple() override {
        mod0->remove_dependency(this);
        mod1->remove_dependency(this);
    }

    void active_float_changed(IVP_U_Active_Float *) override {
        if (last_update == change_meter)
            return;
        last_update = change_meter;
        const IVP_DOUBLE value = mod0->give_double_value() +
            factor * mod1->give_double_value();
        if (value != double_value) {
            double_value = value;
            update_derived();
        }
    }

    int print() override {
        std::printf("(");
        mod0->print();
        std::printf(") + %g * (", factor);
        mod1->print();
        std::printf(")");
        return 0;
    }

private:
    IVP_U_Active_Float *mod0;
    IVP_U_Active_Float *mod1;
    IVP_DOUBLE factor;
};

class IVP_U_Active_Mult : public IVP_U_Active_Float,
                          public IVP_U_Active_Float_Listener {
public:
    IVP_U_Active_Mult(const char *name, IVP_U_Active_Float *left,
                      IVP_U_Active_Float *right)
        : IVP_U_Active_Float(name), mod0(left), mod1(right) {
        mod0->add_dependency(this);
        mod1->add_dependency(this);
        active_float_changed(this);
    }

    ~IVP_U_Active_Mult() override {
        mod0->remove_dependency(this);
        mod1->remove_dependency(this);
    }

    void active_float_changed(IVP_U_Active_Float *) override {
        if (last_update == change_meter)
            return;
        last_update = change_meter;
        const IVP_DOUBLE value =
            mod0->give_double_value() * mod1->give_double_value();
        if (value != double_value) {
            double_value = value;
            update_derived();
        }
    }

    int print() override {
        std::printf("(");
        mod0->print();
        std::printf(") * (");
        mod1->print();
        std::printf(")");
        return 0;
    }

private:
    IVP_U_Active_Float *mod0;
    IVP_U_Active_Float *mod1;
};

class IVP_U_Active_Limit : public IVP_U_Active_Float,
                           public IVP_U_Active_Float_Listener {
public:
    IVP_U_Active_Limit(const char *name, IVP_U_Active_Float *value,
                       IVP_DOUBLE lowValue, IVP_DOUBLE highValue)
        : IVP_U_Active_Float(name), mod(value), low_val(lowValue),
          high_val(highValue) {
        mod->add_dependency(this);
        active_float_changed(this);
    }

    ~IVP_U_Active_Limit() override { mod->remove_dependency(this); }

    void active_float_changed(IVP_U_Active_Float *) override {
        if (last_update == change_meter)
            return;
        last_update = change_meter;
        IVP_DOUBLE value = mod->give_double_value();
        if (value < low_val)
            value = low_val;
        if (value > high_val)
            value = high_val;
        if (value != double_value) {
            double_value = value;
            update_derived();
        }
    }

    int print() override {
        std::printf("Limit[%g, %g](", low_val, high_val);
        mod->print();
        std::printf(")");
        return 0;
    }

private:
    IVP_U_Active_Float *mod;
    IVP_DOUBLE low_val;
    IVP_DOUBLE high_val;
};

class IVP_U_Active_Test_Range : public IVP_U_Active_Int,
                                public IVP_U_Active_Float_Listener {
public:
    IVP_U_Active_Test_Range(const char *name, IVP_U_Active_Float *test,
                            IVP_U_Active_Float *low,
                            IVP_U_Active_Float *high)
        : IVP_U_Active_Int(name), mod_test(test), mod_low_val(low),
          mod_high_val(high) {
        mod_test->add_dependency(this);
        mod_low_val->add_dependency(this);
        mod_high_val->add_dependency(this);
        active_float_changed(test);
    }

    ~IVP_U_Active_Test_Range() override {
        mod_test->remove_dependency(this);
        mod_low_val->remove_dependency(this);
        mod_high_val->remove_dependency(this);
    }

    void active_float_changed(IVP_U_Active_Float *) override {
        if (last_update == IVP_U_Active_Float::change_meter)
            return;
        last_update = IVP_U_Active_Float::change_meter;
        const IVP_DOUBLE testValue = mod_test->give_double_value();
        const int value =
            testValue >= mod_low_val->give_double_value() &&
            testValue <= mod_high_val->give_double_value()
            ? 1
            : 0;
        if (value != int_value) {
            int_value = value;
            update_derived();
        }
    }

    int print() override {
        std::printf("TestRange[");
        mod_low_val->print();
        std::printf(", ");
        mod_high_val->print();
        std::printf("](");
        mod_test->print();
        std::printf(")");
        return 0;
    }

private:
    IVP_U_Active_Float *mod_test;
    IVP_U_Active_Float *mod_low_val;
    IVP_U_Active_Float *mod_high_val;
};

class IVP_U_Active_Switch : public IVP_U_Active_Float,
                            public IVP_U_Active_Float_Listener,
                            public IVP_U_Active_Int_Listener {
public:
    IVP_U_Active_Switch(const char *name, IVP_U_Active_Int *condition,
                        IVP_U_Active_Float *whenTrue,
                        IVP_U_Active_Float *whenFalse)
        : IVP_U_Active_Float(name), mod_cond(condition),
          mod_true(whenTrue), mod_false(whenFalse) {
        mod_cond->add_dependency(this);
        mod_true->add_dependency(this);
        mod_false->add_dependency(this);
        active_float_changed(whenTrue);
    }

    ~IVP_U_Active_Switch() override {
        mod_cond->remove_dependency(this);
        mod_true->remove_dependency(this);
        mod_false->remove_dependency(this);
    }

    void active_float_changed(IVP_U_Active_Float *) override {
        if (last_update == IVP_U_Active_Float::change_meter)
            return;
        last_update = IVP_U_Active_Float::change_meter;
        const IVP_DOUBLE value = mod_cond->give_int_value()
            ? mod_true->give_double_value()
            : mod_false->give_double_value();
        if (value != double_value) {
            double_value = value;
            update_derived();
        }
    }

    void active_int_changed(IVP_U_Active_Int *) override {
        active_float_changed(nullptr);
    }

    int print() override {
        std::printf("Cond[");
        mod_true->print();
        std::printf(", ");
        mod_false->print();
        std::printf("](");
        mod_cond->print();
        std::printf(")");
        return 0;
    }

private:
    IVP_U_Active_Int *mod_cond;
    IVP_U_Active_Float *mod_true;
    IVP_U_Active_Float *mod_false;
};

inline IVP_U_Active_Float *
IVP_U_Active_Value_Manager::get_active_float_by_name(const char *name) {
    if (!name)
        return nullptr;

    search_active_value->name = const_cast<char *>(name);
    auto *value = static_cast<IVP_U_Active_Float *>(
        floats_name_hash->find_active_value(search_active_value));
    search_active_value->name = nullptr;
    if (value)
        return value;

    int position = 0;
    if (name[position] == '-')
        ++position;
    if (name[position] == '.')
        ++position;
    if (name[position] < '0' || name[position] > '9')
        return nullptr;

    value = new IVP_U_Active_Terminal_Double(name, std::atof(name));
    insert_active_float(value);
    return value;
}

inline IVP_U_Active_Int *
IVP_U_Active_Value_Manager::get_active_int_by_name(const char *name) {
    if (!name)
        return nullptr;

    search_active_value->name = const_cast<char *>(name);
    auto *value = static_cast<IVP_U_Active_Int *>(
        ints_name_hash->find_active_value(search_active_value));
    search_active_value->name = nullptr;
    if (value)
        return value;

    int position = 0;
    if (name[position] == '-')
        ++position;
    if (name[position] == '.')
        ++position;
    if (name[position] < '0' || name[position] > '9')
        return nullptr;

    value = new IVP_U_Active_Terminal_Int(name, std::atoi(name));
    insert_active_int(value);
    return value;
}

struct BML_IvpActiveValueManagerLayoutCheck {
    static constexpr std::size_t delete_flag =
        offsetof(IVP_U_Active_Value_Manager, delete_on_env_delete);
    static constexpr std::size_t float_hash =
        offsetof(IVP_U_Active_Value_Manager, floats_name_hash);
    static constexpr std::size_t delayed_floats =
        offsetof(IVP_U_Active_Value_Manager, delayed_active_floats);
    static constexpr std::size_t delayed_ints =
        offsetof(IVP_U_Active_Value_Manager, delayed_active_ints);
    static constexpr std::size_t current_time =
        offsetof(IVP_U_Active_Value_Manager, mod_current_time);
    static constexpr std::size_t search_value =
        offsetof(IVP_U_Active_Value_Manager, search_active_value);
};

struct BML_IvpActiveValueLayoutCheck {
    static constexpr std::size_t name = offsetof(IVP_U_Active_Value, name);
    static constexpr std::size_t references =
        offsetof(IVP_U_Active_Value, reference_count);
};

struct BML_IvpActiveTerminalDoubleLayoutCheck {
    static constexpr std::size_t old_value =
        offsetof(IVP_U_Active_Terminal_Double, old_value);
};

struct BML_IvpActiveTerminalIntLayoutCheck {
    static constexpr std::size_t old_value =
        offsetof(IVP_U_Active_Terminal_Int, old_value);
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_U_Active_Float_Listener) == 0x04);
static_assert(sizeof(IVP_U_Active_Int_Listener) == 0x04);
static_assert(sizeof(IVP_U_Active_Float_Delayed) == 0x04);
static_assert(sizeof(IVP_U_Active_Int_Delayed) == 0x04);
static_assert(sizeof(IVP_U_Active_Value_Manager) == 0x28);
static_assert(BML_IvpActiveValueManagerLayoutCheck::delete_flag == 0x04);
static_assert(BML_IvpActiveValueManagerLayoutCheck::float_hash == 0x08);
static_assert(BML_IvpActiveValueManagerLayoutCheck::delayed_floats == 0x10);
static_assert(BML_IvpActiveValueManagerLayoutCheck::delayed_ints == 0x18);
static_assert(BML_IvpActiveValueManagerLayoutCheck::current_time == 0x20);
static_assert(BML_IvpActiveValueManagerLayoutCheck::search_value == 0x24);
static_assert(sizeof(IVP_U_Active_Value) == 0x0C);
static_assert(BML_IvpActiveValueLayoutCheck::name == 0x04);
static_assert(BML_IvpActiveValueLayoutCheck::references == 0x08);
static_assert(sizeof(IVP_Active_Value_Hash) == 0x10);
static_assert(sizeof(IVP_U_Active_Float) == 0x28);
static_assert(sizeof(IVP_U_Active_Int) == 0x20);
static_assert(sizeof(IVP_U_Active_Terminal_Double) == 0x38);
static_assert(BML_IvpActiveTerminalDoubleLayoutCheck::old_value == 0x30);
static_assert(sizeof(IVP_U_Active_Terminal_Int) == 0x28);
static_assert(BML_IvpActiveTerminalIntLayoutCheck::old_value == 0x24);
static_assert(sizeof(IVP_U_Active_Sine) == 0x50);
static_assert(sizeof(IVP_U_Active_Square) == 0x48);
static_assert(sizeof(IVP_U_Active_Pulse) == 0x50);
static_assert(sizeof(IVP_U_Active_Add) == 0x38);
static_assert(sizeof(IVP_U_Active_Sub) == 0x38);
static_assert(sizeof(IVP_U_Active_Add_Multiple) == 0x40);
static_assert(sizeof(IVP_U_Active_Mult) == 0x38);
static_assert(sizeof(IVP_U_Active_Limit) == 0x40);
static_assert(sizeof(IVP_U_Active_Test_Range) == 0x30);
static_assert(sizeof(IVP_U_Active_Switch) == 0x40);
static_assert(offsetof(IVP_U_Active_Float, last_update) == 0x18);
static_assert(offsetof(IVP_U_Active_Float, double_value) == 0x20);
static_assert(offsetof(IVP_U_Active_Int, last_update) == 0x18);
static_assert(offsetof(IVP_U_Active_Int, int_value) == 0x1C);
#endif

#endif // BML_IVP_ACTIVE_VALUE_H
