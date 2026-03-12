<!-- markdownlint-disable MD009 MD012 MD022 MD031 MD032 MD037 MD040 MD060 -->

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

### 1.1 **Lua Process API** (Threads/Coroutines Isoladas)
```lua
-- Objetivo: cada "processo" é um Lua coroutine/thread isolado
local proc = process.create("script_content")
process.list()         -- lista processos
process.kill(pid)      -- mata processo
process.yield()        -- cede tempo
process.get_id()       -- ID do processo
```
- [x] Estrutura de Process/Coroutine com contexto isolado (cooperativo)
- [x] Tabela global isolada por processo (_ENV por chunk)
- [x] Process manager no kernel (create/list/kill/get_id)
- [x] Scheduler que roda processos Lua via coroutines (tick cooperativo)
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
- [x] Paginação ativa + page fault handler (#PF com addr+err, per-process CR3 switch)
- [x] Proteção de memória (RW+NX enforcement MMU em 4KB via kmemory)
- [x] API Lua para consultar estado de memória (memory.dump_stats)
- [x] API Lua para allocate/free/protect (backend heap + flags)
- [x] Demand paging / recuperação de page faults (não-present mapeado sob demanda, sem halt global)
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
- [x] Estender API kfs atual (aliases fs.* + kfs.*)
- [x] Persistence: garantir dados sobrevivem reboot
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
- [x] Mutexes/spinlocks acessíveis via Lua
- [x] Compatibilidade de API lock/unlock e acquire/release
- [x] Proteção contra race conditions
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
- [x] APIs para informações de sistema (version, uptime_ms/us, memory, cpu_count)
- [x] Identificação de CPU (threads lógicas + cores físicas + vendor/brand)
- [x] Mapa de memória firmware (E820) exposto para Lua (count + entries)
- [x] Comandos `sysstats` e `memmap` com saída detalhada
- [x] Separação explícita entre RAM utilizável, heap e ROM/boot media
- [x] PIT timer interrupt-driven (100Hz) com uptime via `sys.uptime_ms()`
- **Impacto**: Lua pode fazer coisas dependentes de tempo
- **Esforço**: Baixo-Médio | **Importância**: 7/10

### 1.6 **Boot Logging API**
```c
// Objetivo: logs de inicialização padronizados e legíveis
kbootlog_line("boot", "running scheduler ticks");
kbootlog_line("selftest", "process scheduler armed");
```
- [x] API dedicada de boot log (`kbootlog_title`, `kbootlog_write`, `kbootlog_writeln`, `kbootlog_line`)
- [x] Título do log em azul na VGA com conteúdo em branco
- [x] Espelhamento automático para serial em todas as linhas de boot
- [x] Contagem regressiva antes de limpar tela e abrir `kterm`
- **Impacto**: Diagnóstico de boot consistente em VGA+serial
- **Esforço**: Baixo | **Importância**: 8/10

---

## 2. IMPORTANTE (Próximas Semanas - Kernel Foundation)

### 2.1 **Paginação e Proteção de Memória (suporta process.create)**
- [x] Paging ativo (identity-mapped 2MB em kernel_entry.asm, split para 4KB sob demanda em kmemory)
- [x] Page fault handler (#PF) com endereço de falha e error code detalhados (VGA+serial)
- [x] Per-process page tables (PML4/PDPT/PD por processo, CR3 switch no scheduler)
- [x] Proteção read/write/exec bits (RW+NX enforcement em 4KB via kmemory_apply_protection_4k)
- [x] Demand paging / recuperação de page faults (handler tenta recuperação para faltas not-present)
- **Impacto**: Isolamento entre processos Lua
- **Esforço**: Alto | **Importância**: 10/10

### 2.2 **Scheduler Básico (suporta process.yield)**
- [x] Round-robin scheduler cooperativo para Lua coroutines (kprocess_tick)
- [x] PIT timer interrupt (100Hz) com contagem de ticks pendentes
- [x] Context switching entre processos Lua (CR3 switch + lua_resume por processo)
- [x] Preemption sem yield explícito (timer seta flag mas não força context switch)
- [x] Scheduler integrado ao loop do terminal (atualmente roda ticks manuais no boot)
- **Impacto**: Processo Lua 1 + Processo Lua 2 rodam "simultaneamente"
- **Esforço**: Alto | **Importância**: 9/10

### 2.3 **Drivers Básicos para Lua**
- [x] PS/2 Keyboard driver (polling com scancode→ASCII, shift support)
- [x] PS/2 Keyboard → Lua input API (tabela `keyboard.*` em Lua)
- [x] PIT Timer interrupt-driven (100Hz, ktimer com ksys_tick + kprocess_request_tick)
- [x] Serial melhorado (interrupt-driven)
- [x] VGA terminal com cursor hardware sincronizado + scroll automático
- [x] VGA com suporte a ANSI codes
- **Impacto**: Input real + timing preciso
- **Esforço**: Médio | **Importância**: 7/10

### 2.4 **Melhor Tratamento de Exceções**
- [x] Handler #PF detalhado (endereço + error code em hex, VGA+serial)
- [x] Handler #DE (division error) com halt
- [x] Handlers para #GP, #DF detalhados
- [x] Stack trace/backtrace
- [x] Lua panic handler com debugging
- **Impacto**: Diagnosticar erro em scripts fica fácil
- **Esforço**: Médio | **Importância**: 7/10

---

## 3. RECOMENDADO (Médio Prazo - Storage & Advanced Features)

### 3.1 **Disco Persistente (suporta fs.mount)**
- [x] Driver ATA/IDE básico
- [x] Filesystem simples (PLFS customizado + format lib PLF)
- [x] Lua filesystem.lua boot script (init.lua auto-run)
- [x] Init system em Lua
- **Impacto**: Dados persistem entre boots, init customizável
- **Esforço**: Alto | **Importância**: 8/10

### 3.2 **Lua Event/Message System**
```lua
event.subscribe("key_pressed", function(key) ... end)
event.subscribe("timer", function() ... end)
event.emit("custom_event", data)
```
- [x] Event queue e dispatcher
- [x] Callback system Lua
- [x] Built-in events: keyboard, timer, I/O
- **Impacto**: Aplicações reativas, event-driven
- **Esforço**: Médio | **Importância**: 6/10

### 3.3 **Lua Security & Permissions**
```lua
process.getuid()            -- user ID
process.setuid(uid)         -- mudar user
fs.chmod(file, mode)        -- permissões
process.get_capabilities()  -- capabilities
```
- [x] User/Group system primitivo
- [x] File permissions (rwx para user/group/other)
- [x] Capabilities-based security
- **Impacto**: Multi-user seguro
- **Esforço**: Médio-Alto | **Importância**: 6/10

### 3.4 **Lua Debugging/REPL Melhorado**
- [x] Debugger built-in (breakpoints, step, watch)
- [x] REPL com history
- [x] Profiler de performance
- [x] Memory inspector
- **Impacto**: Development experience muito melhor
- **Esforço**: Médio | **Importância**: 5/10

### 3.5 **Boot Checkup Completo**
- [x] Verificação de integridade do heap (blocos, fragmentação)
- [x] Verificação de IDT (handlers registrados para exceções críticas)
- [x] Verificação de disco/ATA (leitura de teste, status do storage)
- [x] Verificação de Lua VM (estado válido, APIs registradas)
- [x] Verificação de filesystem (contagem de arquivos, consistência)
- [x] Verificação de timer/PIT (ticks incrementando)
- [x] Relatório de checkup com status pass/fail antes de abrir terminal
- **Impacto**: Detecção de problemas no boot antes de expor o terminal
- **Esforço**: Médio | **Importância**: 7/10

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

### 4.4 **Lua Language Server (LSP) no Kernel**
```lua
-- Objetivo: autocomplete, diagnósticos e hover para scripts Lua dentro do kernel
-- Core em C (performance), exposto como API Lua
local completions = lsp.complete("fs.wr", 5)  -- posição do cursor
local diags = lsp.check("local x = 1 +")      -- erros/warnings
```
- [ ] Lexer/tokenizer Lua em C (rápido, sem alocação dinâmica onde possível)
- [ ] Analisador de escopo/symbols em C (resolve globals, locals, upvalues)
- [ ] Engine de completions em C (tabelas conhecidas: fs.*, process.*, sys.*, etc.)
- [ ] API Lua para acesso: `lsp.complete()`, `lsp.check()`, `lsp.hover()`
- [ ] Protocolo LSP subset via serial (JSON-RPC bridge para editor externo)
- [ ] Integração com VS Code via extensão serial bridge
- **Impacto**: Dev experience profissional com performance nativa
- **Esforço**: Muito Alto | **Importância**: 4/10

### 4.5 **Virtualização**
- [ ] VMX/SVM (Intel/AMD virtualization)
- [ ] Hypervisor primitivo
- [ ] VMs simples
- **Impacto**: Rodar múltiplos SOs
- **Esforço**: Extremo | **Importância**: 2/10

---

## Sugestão: Próximo Passo Recomendado

Com paginação, proteção de memória, scheduler cooperativo e PIT já funcionando, os próximos passos são:

1. **Preemption real**: Timer IRQ forçar context switch (não apenas setar flag)
2. **Scheduler no kterm**: Integrar `kprocess_tick` ao loop do terminal
3. **Demand paging**: Page fault handler recuperar faltas em vez de halt
4. **Keyboard Lua API**: Expor input do teclado como tabela Lua
5. **Disco persistente**: Driver ATA/IDE + filesystem simples

Isso evoluiria o kernel de cooperativo para preemptivo com I/O real.

---

## Checklist Atual vs. Roadmap

```
[x] Boot x86_64 long mode
[x] IDT/ISRs (#DE, #PF com endereço+error code, IRQ0)
[x] VGA + Serial I/O (cursor hardware + scroll)
[x] Heap allocator (kmalloc/kfree + kheap_total_bytes)
[x] Lua VM integrada
[x] Filesystem em memória (kfs + alias fs.*)
[x] Process API Lua (create/list/kill/yield/get_id)
[x] Scheduler cooperativo de coroutines Lua (round-robin)
[x] Isolamento de ambiente global por processo Lua (_ENV + metatable)
[x] Memory API Lua (allocate/free/protect/dump_stats)
[x] Page fault handler (#PF) detalhado (addr+err, VGA+serial, halt)
[x] Paginação ativa (2MB identity-mapped, split 4KB sob demanda)
[x] Proteção de memória RW+NX enforcement em 4KB (kmemory)
[x] Per-process page tables com CR3 switch
[x] PIT timer interrupt-driven (100Hz, ktimer)
[x] Terminal interativo (kterm) na VGA com input keyboard+serial
[x] Entrada de teclado PS/2 (polling, shift, scancode→ASCII)
[x] Cursor VGA sincronizado (hardware cursor)
[x] Security hardening (v0.1)
[x] Boot logging API unificada (VGA+serial, kbootlog)
[x] Contagem regressiva pós-selftest antes do terminal
[x] E820 map para Lua (sys.memory_map_* + comando memmap)
[x] CPU vendor/brand e cores físicas em sysstats
[x] Preemption real (timer não força context switch)
[x] Demand paging (page fault → recuperação)
[x] Scheduler integrado ao loop do terminal
[ ] System calls
[x] Disco persistente (PLFS + format lib PLF + init.lua)
[x] Serial interrupt-driven
[x] Input API Lua para teclado
[x] VGA ANSI codes
[x] ROM total e tamanho da imagem do kernel reportados separadamente
[x] #GP e #DF handlers detalhados com registradores
[x] Stack backtrace (RBP chain walk)
[x] Lua panic/error com traceback
[ ] User/Group system + permissões
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
| **Modelo de concorrência** | Preemption + signal handling | Scheduler de coroutines + yield explícito |
| **Overhead** | Baixo (syscall direto) | Médio (Lua → C → hardware) |
| **Flexibilidade** | Moderada (compilar código novo) | Alta (reconfigurar em Lua em runtime) |
| **Objetivos** | Performance + compatibilidade | Segurança + simples + introspectável |

---

## 6. PLANO DE SPRINT (6 Semanas Recomendadas)

### **Week 1-2: Paginação (Prerequisito)**
**Objetivo**: Ativar paging + page fault handler  
**Tarefas**:
- [x] Ativar paging em kernel_entry.asm (2MB identity mapping)
- [x] Implementar page table allocator/split em C (kmemory, 4KB sob demanda)
- [x] Registrar handler para #PF (INT 14)
- [x] Testar: criar 2 page tables, mapear memória, trocar CR3 e confirmar isolamento básico
**Saídas**: Cada processo Lua terá sua própria página table  
**Bloqueador para**: Todo o resto (memory protection = pré-requisito)

### **Week 3: Process Manager + Coroutine Binding**
**Objetivo**: Implementar `process.create()`, `process.list()`, etc  
**Tarefas**:
- [~] Estrutura TCB (Task Control Block) em C (base pronta)
  - PID, lua_State*, coroutine ref, page_table_ptr, state (RUNNING/BLOCKED/DEAD)
- [x] Process manager: create, list, kill, yield, get_id
- [x] Registrar global `process` Lua table durante klua_init
  - `process.create(script_content)` → aloca TCB, cria coroutine, retorna PID
  - `process.list()` → retorna Lua table {pid1={state=...}, pid2={...}, ...}
  - `process.kill(pid)` → marca como DEAD, libera resources
  - `process.yield()` → cede scheduler
- [x] Testar: criar 2 scripts Lua via process.create, ambos rodam (cooperativo)
**Saídas**: Múltiplos scripts Lua rodam isolados (coroutines + diferentes page tables)

### **Week 4: Scheduler Basic + PIT Timer**
**Objetivo**: Round-robin scheduler, time-slice básico  
**Tarefas**:
- [x] Implementar PIT timer interrupt (IRQ0, 100Hz padrão = 10ms time slice)
  - ktimer_init(frequency) em C
  - irq0 em asm que chama ktimer_irq_handler
- [x] Scheduler cooperativo C function: kprocess_tick()
  - Itera processos RUNNING (round-robin)
  - Faz CR3 switch por processo quando habilitado
  - Executa lua_resume por processo pronto
- [~] Context save/restore completo de CPU (RIP/RSP/regs) por preemption de timer
- [x] Testar: 2+ scripts Lua rodando em round-robin cooperativo
**Saídas**: Time-sharing funcional; sistemas operacionais "clássicos"

### **Week 5: Drivers + Input/Output Real**
**Objetivo**: PS/2 keyboard, serial melhorado, ANSI VGA  
**Tarefas**:
- [x] PS/2 keyboard driver básico (polling)
- [ ] PS/2 keyboard interrupt-driven
  - [ ] input buffer + keyscan normalization
  - [ ] Registrar Lua `input.read_key()`, `input.available()` ou event system
- [~] PIT timer exposto para Lua (`sys.uptime_ms/us` pronto; `sys.get_time()` pendente)
- [ ] Serial interrupt-driven (INT 4, COM1)
  - Preenche buffer em ISR, kterm lê async
- [x] Cursor VGA no fim da linha (hardware cursor)
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
> - Performance importa: lógica crítica em C como lib Lua, APIs em Lua para ergonomia.

---

## Métricas de Sucesso

| Milestone | Critério |
|-----------|----------|
| v0.2 | Paging + Scheduler + 2+ processos Lua rodando isolados |
| v0.3 | Input/Output real (keyboard + timer) + init.lua |
| v0.4 | Disco ATA básico + filesystem persistente |
| v0.5 | Event system + IPC (message queues) |
| v1.0 | Kernel "estável" com aplicações reais routable Lua |

