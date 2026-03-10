#ifndef STDDEF_H
#define STDDEF_H

/*
 tipo usado para representar tamanhos de memória
 normalmente é um inteiro sem sinal do tamanho da arquitetura
*/
typedef unsigned long size_t;

/*
 ponteiro nulo
*/
#define NULL ((void*)0)

/*
 diferença entre dois ponteiros
*/
typedef long ptrdiff_t;

#endif