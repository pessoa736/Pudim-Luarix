# Roadmap de Kernel - Prioridades
## Design: Lua-First Kernel (não Unix-like)

O Pudim-Luarix é um kernel onde **Lua é a linguagem de sistema**. Tudo expõe APIs Lua, não syscalls tradicionais.

```
┌─────────────────────────────────────┐
│  Lua Scripts (aplicações)           │
├─────────────────────────────────────┤
│  Lua APIs (process, fs, sync, ...)  │
├─────────────────────────────────────┤
│  Kernel Services (paging, sched)    │
├─────────────────────────────────────┤
│  Hardware (x86_64, VGA, serial)     │
└─────────────────────────────────────┘
```

## 1. CRÍTICO (Fase Atual - Lua Integration)

## 1. CRÍTICO (Fase Atual - Lua Integration)

### 1.1 **Lua Process API** (Threads/Coroutines Isoladas)
```lua
-- Objetivo: cada "processo" é um Lua coroutine/thread isolado
local proc = process.create("script_content")
process.list()         -- lista processos
process.kill(pid)      -- mata processo
process.yield()        -- cede tempo
process.get_id()       -- ID do processo
```
- [ ] Estrutura de Process/Coroutine com contexto isolado
- [ ] Tabela global isolada por processo
- [ ] Process manager no kernel
- [ ] Scheduler que roda processos Lua via coroutines
- **Impacto**: Múltiplas aplicações Lua isoladas simultâneas
- **Esforço**: Alto | **Importância**: 10/10

### 1.2 **Lua Memory Protection API**
```lua
-- Objetivo: paginação exposta como API Lua
memory.allocate(size)        -- aloca página
memory.free(ptr)             -- libera
memory.protect(area, flags)  -- read/write/exec
memory.dump_stats()          -- info de memória
```
- [ ] Paginação ativa + page fault handler
- [ ] Proteção de memória (read/write/exec bits)
- [ ] API Lua para consultar estado de memória
- **Impacto**: Isolamento real entre processos
- **Esforço**: Alto | **Importância**: 10/10

### 1.3 **Lua Filesystem API Melhorado**
```lua
-- Já tem kfs, mas expandir com:
fs.write(name, content)
fs.read(name)
fs.delete(name)
fs.list()
fs.size(name)
fs.mount(device, path)      -- (futuro)
fs.chmod(name, perms)       -- (futuro)
```
- [ ] Estender API kfs atual
- [ ] Persistence: garantir dados sobrevivem reboot
- [ ] Quotas de espaço por processo (futuro)
- **Impacto**: Lua pode salvar/carregar estados
- **Esforço**: Médio | **Importância**: 9/10

### 1.4 **Lua Synchronization API**
```lua
-- Para multi-process safety
lock = sync.mutex()
lock:acquire()
lock:release()

-- ou
lock = sync.spinlock()
lock:lock()
lock:unlock()
```
- [ ] Mutexes/semaphores acessíveis via Lua
- [ ] Proteção contra race conditions
- [ ] Deadlock detection (futuro)
- **Impacto**: Código Lua seguro em multitarefa
- **Esforço**: Médio | **Importância**: 8/10

### 1.5 **Lua System Info API**
```lua
sys.version()          -- "Pudim-Luarix v0.1.0"
sys.uptime()           -- segundos desde boot
sys.get_time()         -- clock atual
sys.cpu_count()        -- número de CPUs
sys.memory_total()     -- RAM total
sys.memory_free()      -- RAM livre
```
- [ ] APIs para informações de sistema
- [ ] Clock/Timer (importante para scheduler)
- **Impacto**: Lua pode fazer coisas dependentes de tempo
- **Esforço**: Baixo-Médio | **Importância**: 7/10

---

## 2. IMPORTANTE (Próximas Semanas - Kernel Foundation)

## 2. IMPORTANTE (Próximas Semanas - Kernel Foundation)

### 2.1 **Paginação e Proteção de Memória (suporta process.create)**
- [ ] Ativar paging (já tem setup em kernel_entry.asm)
- [ ] Page fault handler com erro graceful
- [ ] Per-process page tables
- [ ] Proteção read/write/exec bits
- **Impacto**: Isolamento entre processos Lua
- **Esforço**: Alto | **Importância**: 10/10

### 2.2 **Scheduler Básico (suporta process.yield)**
- [ ] Round-robin scheduler para Lua coroutines
- [ ] PIT timer interrupt (100Hz default)
- [ ] Context switching entre processos Lua
- [ ] Preemption sem yield explícito (futuro)
- **Impacto**: Processo Lua 1 + Processo Lua 2 rodam "simultaneamente"
- **Esforço**: Alto | **Importância**: 9/10

### 2.3 **Drivers Básicos para Lua**
- [ ] PS/2 Keyboard → Lua input API
- [ ] PIT Timer → Lua sys.get_time()
- [ ] Serial melhorado (interrupt-driven)
- [ ] VGA com suporte a ANSI codes
- **Impacto**: Input real + timing preciso
- **Esforço**: Médio | **Importância**: 7/10

### 2.4 **Melhor Tratamento de Exceções**
- [ ] Handlers para #GP, #PF, #DF detalhados
- [ ] Stack trace/backtrace
- [ ] Lua panic handler com debugging
- **Impacto**: Diagnosticar erro em scripts fica fácil
- **Esforço**: Médio | **Importância**: 7/10

---

## 3. RECOMENDADO (Médio Prazo - Storage & Advanced Features)

### 3.1 **Disco Persistente (suporta fs.mount)**
- [ ] Driver ATA/IDE básico
- [ ] Filesystem simples (FAT12 ou customizado)
- [ ] Lua filesystem.lua boot script
- [ ] Init system em Lua
- **Impacto**: Dados persistem entre boots, init customizável
- **Esforço**: Alto | **Importância**: 8/10

### 3.2 **Lua Event/Message System**
```lua
event.subscribe("key_pressed", function(key) ... end)
event.subscribe("timer", function() ... end)
event.emit("custom_event", data)
```
- [ ] Event queue e dispatcher
- [ ] Callback system Lua
- [ ] Built-in events: keyboard, timer, I/O
- **Impacto**: Aplicações reativas, event-driven
- **Esforço**: Médio | **Importância**: 6/10

### 3.3 **Lua Security & Permissions**
```lua
process.getuid()            -- user ID
process.setuid(uid)         -- mudar user
fs.chmod(file, mode)        -- permissões
process.get_capabilities()  -- capabilities
```
- [ ] User/Group system primitivo
- [ ] File permissions (rwx para user/group/other)
- [ ] Capabilities-based security
- **Impacto**: Multi-user seguro
- **Esforço**: Médio-Alto | **Importância**: 6/10

### 3.4 **Lua Debugging/REPL Melhorado**
- [ ] Debugger built-in (breakpoints, step, watch)
- [ ] REPL com history
- [ ] Profiler de performance
- [ ] Memory inspector
- **Impacto**: Development experience muito melhor
- **Esforço**: Médio | **Importância**: 5/10

---

## 4. FUTURO (Longo Prazo - Enterprise Features)

### 4.1 **Rede (TCP/IP Stack)**
- [ ] Driver de NIC
- [ ] Camadas de link/IP/TCP
- [ ] Socket API
- **Impacto**: Comunicação entre máquinas
- **Esforço**: Muito Alto | **Importância**: 5/10

### 4.2 **Pipe/IPC**
- [ ] Named/unnamed pipes
- [ ] Message queues
- [ ] Shared memory
- **Impacto**: Processos podem comunicar
- **Esforço**: Médio | **Importância**: 5/10

### 4.3 **NUMA/SMP**
- [ ] Suporte a múltiplos CPUs
- [ ] Sincronização inter-CPU
- [ ] Load balancing
- **Impacto**: Aproveitar multi-core
- **Esforço**: Muito Alto | **Importância**: 4/10

### 4.4 **Virtualização**
- [ ] VMX/SVM (Intel/AMD virtualization)
- [ ] Hypervisor primitivo
- [ ] VMs simples
- **Impacto**: Rodar múltiplos SOs
- **Esforço**: Extremo | **Importância**: 2/10

---

## Sugestão: Próximo Passo Recomendado

**Paginação + Proteção de Memória** é a base para tudo mais. Depois:

1. **Semana 1-2**: Ativar paging (você já tem setup em kernel_entry.asm)
2. **Semana 3**: Estrutura básica de TCB e task switching
3. **Semana 4**: Scheduler round-robin simples
4. **Semana 5**: Syscalls básicas (exit, write, read)
5. **Semana 6**: Começar driver de disco

Isso te daria um kernel funcional com multitarefa isolada.

---

## Checklist Atual vs. Roadmap

```
[x] Boot x86_64 long mode
[x] IDT/ISRs básicos
[x] VGA + Serial I/O
[x] Heap allocator
[x] Lua VM integrada
[x] Filesystem em memória
[x] Terminal interativo (kterm)
[x] Security hardening (v0.1)
[ ] Paginação ativa
[ ] Proteção de memória
[ ] Task/Process management
[ ] Scheduler
[ ] System calls
[ ] Disco persistente
[ ] Drivers (PS/2, PIT)
```

---

## 5. COMPARAÇÃO: Unix-like vs Lua-First

| Aspecto | Abordagem Unix-like | Abordagem Lua-First |
|--------|-------------------|-------------------|
| **Linguagem** | C/Assembly | Lua (sistema) + C (kernel) |
| **Aplicações** | Binários ELF (C, C++, etc) | Scripts Lua |
| **Interface do Kernel** | Syscalls (fork, exec, open, read) | Tabelas Lua (process.create, fs.write) |
| **Isolamento** | Processos isolados por padrão | Coroutines isoladas (página table isolada) |
| **Interatividade** | Shell + scripts | Lua REPL + scripts |
| **Persistência** | Filesystem tradicional | KFS + disco (futuro) |
| **Model de concorrência** | Preemption + signal handling | Scheduler de coroutines + yield explícito |
| **Overhead** | Baixo (syscall direto) | Médio (Lua → C → hardware) |
| **Flexibilidade** | Moderada (compilar código novo) | Alta (reconfigurar em Lua em runtime) |
| **Objetivos** | Performance + compatibilidade | Segurança + simples + introspectável |

---

## 6. PLANO DE SPRINT (6 Semanas Recomendadas)

### **Week 1-2: Paginação (Prerequisito)**
**Objetivo**: Ativar paging + page fault handler  
**Tarefas**:
- [ ] Ativar PAGING em kernel_entry.asm (ja tem setup basico)
- [ ] Implementar page table allocator em C
- [ ] Registrar handler para #PF (INT 14)
- [ ] Testar: criar 2 página tables, mapear memoria, trocar CR3, confirmar isolamento
**Saídas**: Cada processo Lua terá sua própria página table  
**Bloqueador para**: Todo o resto (memory protection = pré-requisito)

### **Week 3: Process Manager + Coroutine Binding**
**Objetivo**: Implementar `process.create()`, `process.list()`, etc  
**Tarefas**:
- [ ] Estrutura TCB (Task Control Block) em C
  - PID, lua_State*, coroutine ref, page_table_ptr, state (RUNNING/BLOCKED/DEAD)
- [ ] Process manager: create, list, kill, yield, get_id
- [ ] Registrar global `process` Lua table durante klua_init
  - `process.create(script_content)` → aloca TCB, cria coroutine, retorna PID
  - `process.list()` → retorna Lua table {pid1={state=...}, pid2={...}, ...}
  - `process.kill(pid)` → marca como DEAD, libera resources
  - `process.yield()` → cede scheduler
- [ ] Testar: criar 2 scripts Lua via process.create, ambos rodam
**Saídas**: Múltiplos scripts Lua rodam isolados (coroutines + diferentes page tables)

### **Week 4: Scheduler Basic + PIT Timer**
**Objetivo**: Round-robin scheduler, time-slice básico  
**Tarefas**:
- [ ] Implementar PIT timer interrupt (INT 8, 100Hz padrão = 10ms time slice)
  - pit_init(frequency) em C
  - pit_isr em asm que chama scheduler
- [ ] Scheduler C function: sched_tick()
  - Salva estado do processo atual
  - Move para próximo RUNNING process (round-robin)
  - Carrega novo CR3 de página table
  - Restaura contexto (RIP, RSP, etc)
- [ ] Context save/restore (registros, RSP, RIP, CR3)
- [ ] Testar: 2+ scripts rodando "simultaneamente", rodando em turnos 10ms cada
**Saídas**: Time-sharing funcional; sistemas operacionais "clássicos"

### **Week 5: Drivers + Input/Output Real**
**Objetivo**: PS/2 keyboard, serial melhorado, ANSI VGA  
**Tarefas**:
- [ ] PS/2 keyboard driver (INT 1: interrupt handler)
  - kj input buffer, keyscan codes
  - Registrar Lua `input.read_key()`, `input.available()` ou event system
- [ ] PIT timer → `sys.get_time()` (microseconds desde boot)
- [ ] Serial interrupt-driven (INT 4, COM1)
  - Preenche buffer em ISR, kterm lê async
- [ ] VGA ANSI codes (cursor move, colors, etc)
- [ ] Testar: apertar tecla no terminal, input aparece em Lua; timer preciso; cores
**Saídas**: Terminal interativo real + timing preciso para aplicações

### **Week 6: Init System + Persistence Setup**
**Objetivo**: Arrumação final, boot scripts, preparação para disco  
**Tarefas**:
- [ ] Arquivo `/init.lua` em QEMU (embedded via awk no makefile)
  - Boot scripts: carrega/roda aplicações padrão
  - Ex: filesystem check, init processes, REPL
- [ ] Refactor kterm: agora kterm é uma app Lua, não hardcoded
- [ ] Melhorar error handling: tracebacks, panic strings
- [ ] Documentar Lua APIs públicas (versão release-ready)
- [ ] Setup básico driver ATA (não fully implementado, mas interface definida)
- [ ] Testar: boot → load init.lua → exec Lua scripts → kterm REPL funciona
**Saídas**: Kernel "v0.2" com multitarefa Lua estável, pronto para disco/persistência

---

## Filosofia de Design Final

> **Pudim-Luarix é um SO que oferece Lua como linguagem de sistemas.**  
> - Não é Unix clone com Lua bindings.  
> - É um kernel projetado em torno de Lua + isolamento.  
> - APIs não são syscalls; são funções/tabelas Lua que o kernel expõe.  
> - User-space IS Lua-space (não há separação binária; tudo é script).  
> - Performance é secundária a simplicity e debuggability.

---

## Métricas de Sucesso

| Milestone | Critério |
|-----------|----------|
| v0.2 | Paging + Scheduler + 2+ processos Lua rodando isolados |
| v0.3 | Input/Output real (keyboard + timer) + init.lua |
| v0.4 | Disco ATA básico + filesystem persistente |
| v0.5 | Event system + IPC (message queues) |
| v1.0 | Kernel "estável" com aplicações reais routable Lua |

