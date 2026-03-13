# APIs Lua do Kernel

Este mapa resume os modulos Lua expostos pelo kernel, com foco em manutencao e uso no terminal.

Visualmente, da para ler esta pilha como um pudim em camadas: base de runtime (`io`/`serial`), recheio de servicos (`sys`, `fs`, `process`, `memory`) e cobertura de observabilidade (`debug`, `event`).

## Modulos principais

- `sys`: metricas e informacoes da plataforma.
- `fs` (alias `kfs`): arquivos em memoria, persistencia e permissoes.
- `process`: criacao, controle e identidade de processos Lua.
- `memory`: alocacao, liberacao e protecao de memoria.
- `event`: fila de eventos, inscricoes e timer de eventos.
- `sync`: primitivas de sincronizacao (`spinlock` e `mutex`).
- `debug`: introspeccao Lua, historico e perfilador simples.
- `keyboard`: leitura nao bloqueante de teclado.
- `format`: serializacao PLFS e utilitarios de disco.
- `io`/`serial` + funcoes base (`print`, `pairs`, `pcall`, etc.): camada basica de runtime.

## Leitura recomendada

- Comandos do terminal: `docs/commands/terminal.md`
- Comandos Lua dinamicos: `docs/commands/lua-custom.md`
- Referencias por modulo: arquivos em `docs/apis/*.md`
