#include <stdio.h>  // biblioteca padrão para entrada e saída (printf, scanf)

/*
=====================================
FUNÇÃO
Todo programa C começa pela função main
=====================================
*/

int main() {

    /*
    ================================
    1. DECLARAÇÃO DE VARIÁVEIS
    Criamos duas variáveis para guardar números
    ================================
    */

    int numero1;
    int numero2;
    int soma;

    /*
    ================================
    2. SAÍDA DE TEXTO
    printf imprime texto no terminal
    ================================
    */

    printf("Digite o primeiro numero: ");

    /*
    ================================
    3. ENTRADA DE DADOS
    scanf lê um valor digitado pelo usuário
    & significa "endereço da variável"
    ================================
    */

    scanf("%d", &numero1);

    printf("Digite o segundo numero: ");
    scanf("%d", &numero2);

    /*
    ================================
    4. OPERAÇÃO MATEMÁTICA
    Aqui somamos os números
    ================================
    */

    soma = numero1 + numero2;

    /*
    ================================
    5. MOSTRAR RESULTADO
    ================================
    */

    printf("A soma é: %d\n", soma);

    /*
    ================================
    6. RETORNO DO PROGRAMA
    0 significa que o programa terminou corretamente
    ================================
    */

    return 0;
}