#include <stdio.h>
#include <stdlib.h>

/*
========================================
1. MACROS
Macros são substituições feitas pelo pre-processador
antes da compilação.
========================================
*/

#define INITIAL_CAPACITY 4

/*
========================================
2. ENUM
Enum cria um tipo simbólico para estados.
========================================
*/

typedef enum {
    STATUS_OK,
    STATUS_ERROR
} Status;

/*
========================================
3. STRUCT
Struct agrupa dados em uma estrutura única.
Aqui criamos um "objeto" lista dinâmica.
========================================
*/

typedef struct {
    int *data;      // ponteiro para array dinâmico
    size_t size;    // quantidade de elementos
    size_t capacity;// capacidade alocada
} IntList;

/*
========================================
4. FUNÇÃO DE INICIALIZAÇÃO
Inicializa a lista dinâmica.
========================================
*/

Status list_init(IntList *list) {

    list->data = malloc(sizeof(int) * INITIAL_CAPACITY);

    if (list->data == NULL) {
        return STATUS_ERROR;
    }

    list->size = 0;
    list->capacity = INITIAL_CAPACITY;

    return STATUS_OK;
}

/*
========================================
5. REALOCAÇÃO DINÂMICA
Se a lista encher, dobramos o tamanho.
========================================
*/

Status list_expand(IntList *list) {

    size_t new_capacity = list->capacity * 2;

    int *new_data = realloc(list->data, sizeof(int) * new_capacity);

    if (new_data == NULL) {
        return STATUS_ERROR;
    }

    list->data = new_data;
    list->capacity = new_capacity;

    return STATUS_OK;
}

/*
========================================
6. INSERÇÃO
Adiciona elemento na lista.
========================================
*/

Status list_push(IntList *list, int value) {

    if (list->size >= list->capacity) {
        if (list_expand(list) == STATUS_ERROR) {
            return STATUS_ERROR;
        }
    }

    list->data[list->size] = value;
    list->size++;

    return STATUS_OK;
}

/*
========================================
7. CALLBACK
Funções podem ser passadas como argumento.
Isso permite comportamento genérico.
========================================
*/

typedef int (*TransformFunc)(int);

/*
Aplica uma função em todos os elementos da lista
*/

void list_map(IntList *list, TransformFunc func) {

    for (size_t i = 0; i < list->size; i++) {
        list->data[i] = func(list->data[i]);
    }
}

/*
========================================
8. FUNÇÕES DE OPERAÇÃO
Funções que podem ser usadas como callback
========================================
*/

int square(int x) {
    return x * x;
}

int double_value(int x) {
    return x * 2;
}

/*
========================================
9. FUNÇÃO PARA IMPRIMIR
========================================
*/

void list_print(IntList *list) {

    printf("List: ");

    for (size_t i = 0; i < list->size; i++) {
        printf("%d ", list->data[i]);
    }

    printf("\n");
}

/*
========================================
10. LIBERAR MEMÓRIA
========================================
*/

void list_destroy(IntList *list) {
    free(list->data);
}

/*
========================================
11. FUNÇÃO PRINCIPAL
========================================
*/

int main() {

    IntList list;

    if (list_init(&list) == STATUS_ERROR) {
        printf("Erro ao inicializar lista\n");
        return 1;
    }

    // adicionando valores
    for (int i = 1; i <= 5; i++) {
        list_push(&list, i);
    }

    list_print(&list);

    // aplicando operação (callback)
    list_map(&list, square);

    list_print(&list);

    list_map(&list, double_value);

    list_print(&list);

    list_destroy(&list);

    return 0;
}