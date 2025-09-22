# Calculadora Cliente–Servidor com Sockets (C / TCP / IPv4)

## Objetivo
Cliente envia operações matemáticas em **texto** para o servidor via TCP. O servidor **parsa** a linha, executa a operação e devolve o resultado. Conexão fica aberta até `QUIT`.

## Protocolo (texto)
**Requisição (obrigatório, prefixo)**: `OP A B\n`  
Onde `OP ∈ {ADD, SUB, MUL, DIV}` e `A, B` são números decimais com ponto.

**Resposta**  
- Sucesso: `OK R\n` (R em ponto flutuante com `.` – locale fixado para `C`)  
- Erro: `ERR <COD> <mensagem>\n`, onde `<COD> ∈ {EINV, EZDV, ESRV}`

**Bonus implementado**: também aceita **infixo**: `A <op> B\n`, `<op> ∈ {+, -, *, /}`

### Exemplos
```
ADD 10 2        -> OK 12
SUB 7 9         -> OK -2
MUL -3 3.5      -> OK -10.5
DIV 5 0         -> ERR EZDV divisao_por_zero

10 + 2          -> OK 12         # forma infixa (bônus)
QUIT            -> OK bye        # encerra a sessão
```

## Execução
### Compilar
```bash
make         # gera ./server e ./client
```

### Servidor (porta padrão 5050)
```bash
./server           # escuta em 0.0.0.0:5050
./server 6000      # porta customizada
```

### Cliente
```bash
./client 127.0.0.1 5050
ADD 10 2
OK 12
DIV 5 0
ERR EZDV divisao_por_zero
QUIT
OK bye
```

## Decisões de projeto
- **Parsing robusto**: valida nº de tokens, tipos numéricos (`strtod`) e operação; aceita prefixo e infixo (bônus).  
- **Padrão decimal**: `setlocale(LC_NUMERIC, "C")` no servidor para garantir `.` em `double`.  
- **Formatação**: resultado com `%.6f` e **poda** de zeros/trailing `.` para respostas limpas (`12.000000` → `12`).  
- **Erros tratados**: `EINV` (entrada ou operador inválido), `EZDV` (divisão por zero), `ESRV` (falhas internas).  
- **Concorrência (bônus)**: o servidor **forka** por conexão, atendendo múltiplos clientes. Trata `SIGCHLD`.  
- **Encerramento limpo**: `SIGINT` (Ctrl+C) derruba o loop `accept` e fecha o socket listener.

## Estrutura
```
/include
  proto.h
/src
  client.c
  server.c
Makefile
README.md
```

## Testes rápidos
Em dois terminais:
```
# Terminal 1
./server

# Terminal 2
./client 127.0.0.1 5050
ADD 1 2
MUL -3 3.5
DIV 5 0
10 + 2
QUIT
```
Saídas esperadas: `OK 3`, `OK -10.5`, `ERR EZDV divisao_por_zero`, `OK 12`, `OK bye`.

## Limitações
- Conexões **IPv4** apenas (simples e suficiente para a atividade).
- Precisão de ponto flutuante padrão de `double` (não usa `decimal`/`bigint`).
- Sem TLS (propósito didático focado em sockets TCP).

---

**Autor:** Matheus S de Brito - 10408953

**Como avaliar:** use os exemplos acima, rode casos inválidos (tokens faltando, tipos inválidos, `DIV` por zero) e confira respostas `ERR`.
