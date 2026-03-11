# Melhorias de Segurança - Pudim-Luarix

## Problemas Identificados e Corrigidos

### 1. **Integer Overflow em kfs_append**
- **Problema**: Soma de `old_size + add_size` sem verificação de overflow
- **Solução**: Verifica se `old_size > KFS_MAX_FILE_SIZE - add_size` antes de somar
- **Arquivo**: `include/kfs.c`

### 2. **Falta de Limite de Tamanho de Arquivo**
- **Problema**: Arquivos poderiam crescer indefinidamente, consumindo toda a heap
- **Solução**: Adicionado `KFS_MAX_FILE_SIZE = 65536` bytes por arquivo
- **Arquivo**: `include/kfs.c`

### 3. **Buffer Overflow em kterm**
- **Problema**: Buffer de linha com tamanho 128, sem proteção adequada
- **Solução**: 
  - Aumentado para 256 bytes
  - Validação explícita: `if (pos + 1 >= KTERM_BUF_SIZE)` com erro
  - Detecta linhas muito longas e rejeita
- **Arquivo**: `include/kterm.c`

### 4. **Execução de Lua Sem Validação de Comando**
- **Problema**: Nomes de comando aceita qualquer string, incluindo caracteres especiais
- **Solução**: Whitelist de caracteres: `[a-zA-Z0-9_]` apenas
- **Arquivo**: `include/klua_cmds.c`

### 5. **Script Lua Sem Limite de Tamanho**
- **Problema**: Script executado sem limite de tamanho
- **Solução**: 
  - Verifica se script excede `KLUA_CMD_MAX_SCRIPT` (512 bytes)
  - Rejeita execução se tamanho inválido
- **Arquivo**: `include/klua_cmds.c`

## Constantes de Segurança

```c
// Arquivo: include/kfs.c
#define KFS_MAX_FILE_SIZE 65536          // Máx 64KB por arquivo
#define KFS_MAX_TOTAL_SIZE (65536*32)    // Total de ~2MB

// Arquivo: include/kterm.c
#define KTERM_BUF_SIZE 256               // Aumentado de 128
#define KTERM_MAX_CMD_LEN 250            // Comprimento máximo de comando
#define KTERM_TIMEOUT_MS 30000           // Timeout de 30s (futuro)

// Arquivo: include/klua_cmds.c
#define KLUA_CMD_MAX_SCRIPT 512          // Max 512 bytes de script
#define KLUA_CMD_MAX_TOTAL_SIZE 65536    // Max 64KB commands.lua
```

## Recomendações de Segurança Futuras

### 1. **Sandbox Lua**
- [ ] Restringir funções Lua perigosas (os.*, io.*, debug.*)
- [ ] Implementar limites de CPU/memória para scripts
- [ ] Adicionar whitelist de funções permitidas

### 2. **Logging e Auditoria**
- [ ] Registrar todos os comandos Lua executados
- [ ] Registrar operações críticas de filesystem
- [ ] Implementar buffer circular de eventos

### 3. **Autenticação (Futuro)**
- [ ] Adicionar proteção por senha no kterm
- [ ] Implementar rate limiting para evitar brute force
- [ ] Logging de tentativas falhadas

### 4. **Validação de Entrada**
- [ ] Sanitizar strings lidas do filesystem antes de executar como Lua
- [ ] Implementar parser stricter para commands.lua

### 5. **Proteção de Memória**
- [ ] Implementar canários (stack canaries) para detecção de buffer overflow
- [ ] Adicionar bounds checking na heap allocator

## Checklist de Review

- [x] Integer overflow check em append
- [x] Limites de tamanho de arquivo
- [x] Buffer overflow protection em terminal
- [x] Validação de nome de comando
- [x] Verificação de tamanho de script
- [ ] Sandbox Lua (TODO)
- [ ] Rate limiting (TODO)
- [ ] Encrypted storage (TODO)
- [ ] Secure boot validation (TODO)

## Testing

Execute os testes de segurança:
```bash
# Teste de buffer overflow
kterm> write test "$(perl -e 'print "A" x 1000')"

# Teste de comando inválido
kterm> lrun "cmd; rm -rf /"

# Teste de arquivo grande
kterm> append large "$(perl -e 'print "X" x 100000')"
```

Todos devem ser rejeitados ou falharem gracefully.
