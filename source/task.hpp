#ifndef TASK_HPP
#define TASK_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>

enum class TaskSignal{
    NONE,
    SIG_KILL,
    SIG_PAUSE,
    SIG_RESUME
};

enum class TaskState{
    ACTIVE,
    PAUSED,
    DEAD
};





struct TaskContext {
    uint32_t *sp;          // +0
    uint32_t *sp_limit;    // +4
    uint32_t  excReturn;   // +8
};

class Task
{
private:
    TaskContext     m_ctx;
    void           *m_entry;
    uint32_t        m_stack_size;
    uint32_t        m_id;
    TaskSignal      m_signal;
    TaskState       m_state;
    char            m_name[32];

public:

    Task() = default;

    Task(const char * name, void *entry, uint32_t stack_size, uint32_t id)
        : m_ctx{nullptr, nullptr, 0},
          m_entry(entry), m_stack_size(stack_size), m_id(id),
          m_signal(TaskSignal::NONE), m_state(TaskState::ACTIVE) {

            strncpy(m_name, name, sizeof(m_name) - 1);
            m_name[sizeof(m_name) - 1] = '\0';

          }


    TaskContext *getContext()        { return &m_ctx; }
    const char * GetName()     const {return m_name; }
    uint32_t    getId()        const { return m_id; }
    uint32_t    getStackSize() const { return m_stack_size; }
    const void *getEntry()     const { return m_entry; }
    TaskSignal  getSignal()    const { return m_signal; }
    uint32_t   *getSP()        const { return m_ctx.sp; }
    uint32_t   *getSPLimit()   const { return m_ctx.sp_limit; }
    uint32_t    getExcReturn() const { return m_ctx.excReturn; }
    TaskState   getState()     const { return m_state; }
    

    void       SetState(TaskState new_state)         {m_state = new_state; }
    void setSignal(TaskSignal signal)      { m_signal       = signal; }
    void setSP(uint32_t *sp)               { m_ctx.sp        = sp; }
    void setSPLimit(uint32_t *sp_limit)    { m_ctx.sp_limit  = sp_limit; }
    void setExcReturn(uint32_t excReturn)  { m_ctx.excReturn = excReturn; }
};
#endif
