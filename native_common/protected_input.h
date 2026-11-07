#pragma once
#include <atspi/atspi.h>
#include <QObject>
#include <QTimer>
#include <functional>
#include <memory>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstring>

// Only focus, role and state metadata: never request text, values or field names.
class ProtectedInputGuard : public QObject {
    GMainContext *context = nullptr;
    AtspiEventListener *listener = nullptr;
    AtspiAccessible *focused = nullptr;
    QTimer pump;
    std::vector<AtspiAccessible *> protectedFields;
    bool ready = false;
    bool safe = false;
    std::uint64_t generation = 0;
    static constexpr const char *events[] = {
        "object:state-changed:focused", "object:state-changed:editable",
        "object:property-change:accessible-role"
    };
    bool inspect(bool fresh) {
        if (!ready || !focused) return false;
        std::unique_ptr<AtspiAccessible, decltype(&g_object_unref)> object(
            ATSPI_ACCESSIBLE(g_object_ref(focused)), &g_object_unref);
        const auto start = generation;
        if (fresh) atspi_accessible_clear_cache_single(object.get());
        GError *error = nullptr;
        const auto role = atspi_accessible_get_role(object.get(), &error);
        if (error) { g_error_free(error); return false; }
        if (role == ATSPI_ROLE_PASSWORD_TEXT) {
            if(std::find(protectedFields.begin(), protectedFields.end(), object.get()) == protectedFields.end()) {
                if(protectedFields.size() == 64) {
                    ready = false; return false;
                }
                protectedFields.push_back(ATSPI_ACCESSIBLE(g_object_ref(object.get())));
            }
            return false;
        }
        // Keep a revealed password protected; remember only bounded object identities.
        if(std::find(protectedFields.begin(), protectedFields.end(), object.get()) != protectedFields.end()) return false;
        auto states = atspi_accessible_get_state_set(object.get());
        if (!states) return false;
        const bool allowed = atspi_state_set_contains(states, ATSPI_STATE_FOCUSED) &&
            (atspi_state_set_contains(states, ATSPI_STATE_EDITABLE) || role == ATSPI_ROLE_TERMINAL) &&
            !atspi_state_set_contains(states, ATSPI_STATE_DEFUNCT);
        g_object_unref(states);
        return allowed && focused == object.get() && generation == start;
    }
    void refresh() {
        const bool next = inspect(false);
        if (next != safe) { safe = next; if (changed) changed(); }
    }
    static void onAtspiEvent(AtspiEvent *event, void *data) {
        auto self = static_cast<ProtectedInputGuard *>(data);
        ++self->generation;
        if (std::strcmp(event->type, "object:state-changed:focused") == 0) {
            if (event->detail1) {
                if (self->focused) g_object_unref(self->focused);
                self->focused = event->source ? ATSPI_ACCESSIBLE(g_object_ref(event->source)) : nullptr;
            } else if (self->focused == event->source) {
                g_object_unref(self->focused); self->focused = nullptr;
            }
        }
        self->refresh();
        g_boxed_free(ATSPI_TYPE_EVENT, event);
    }
public:
    std::function<void()> changed;
    explicit ProtectedInputGuard(QObject *parent = nullptr) : QObject(parent) {
        context = g_main_context_new(); atspi_set_main_context(context);
        if (atspi_init() != 0) return;
        atspi_set_timeout(75, 75);
        listener = atspi_event_listener_new(&ProtectedInputGuard::onAtspiEvent, this, nullptr);
        ready = listener != nullptr;
        for (const auto name : events) {
            GError *error = nullptr;
            if (!listener || !atspi_event_listener_register(listener, name, &error)) ready = false;
            if (error) g_error_free(error);
        }
        connect(&pump, &QTimer::timeout, this, [this] { drain(); }); pump.start(15);
    }
    void drain() {
        for (unsigned i=0; i<32 && g_main_context_pending(context); ++i)
            g_main_context_iteration(context, false);
    }
    bool allows() { drain(); safe = inspect(true); return safe; }
    ~ProtectedInputGuard() override {
        pump.stop();
        if (listener) {
            for (const auto name : events) {
                GError *error = nullptr;
                atspi_event_listener_deregister(listener, name, &error);
                if (error) g_error_free(error);
            }
            g_object_unref(listener);
        }
        if (focused) g_object_unref(focused);
        for(auto object : protectedFields) g_object_unref(object);
        atspi_exit(); g_main_context_unref(context);
    }
};
