# Pudim-Luarix

Um kernel experimental escrito em C e Assembly, com a intenção de integrar a linguagem Lua diretamente ao sistema, expondo APIs Lua para interagir com o hardware e o sistema operacional. O projeto é movido pela curiosidade de como um sistema pode funcionar nesse contexto.

## Contexto

A ideia surgiu de uma vontade antiga de entender como kernels funcionam por dentro. O ponto de partida foi o vídeo "Tentando fazer um kernel em C e assembly" do canal Lowwryzen no YouTube, que levou ao repositório [laurix](https://github.com/lowwryzen/laurix), usado como base para este projeto.

O nome é uma fusão de Pudim (identidade do projeto), Lua (a linguagem a ser integrada) e rix (herdado do laurix).

## Creditos

- **lowwryzen** — pelo repositório [laurix](https://github.com/lowwryzen/laurix), que serve de base para este projeto, e pelo vídeo que motivou o desenvolvimento.
- **Lua Team** — pela linguagem [Lua](https://www.lua.org/), distribuída sob licença MIT. Lua 5.5.0 é integrada a este kernel para fornecer a camada de sistema.

Este projeto é distribuído sob a licença MIT.

### Licenças de Terceiros

- [Lua License](lua/LICENSE_LUA) — MIT License
- [laurix License](https://github.com/lowwryzen/laurix/blob/main/LICENSE) — MIT License
