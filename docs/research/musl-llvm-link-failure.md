# Falha de Link MUSL com LLVM

Pesquisa em 2026-10-17 sobre o run [38803716931](https://github.com/GalaxyHaze/Zith-Lang/actions/runs/38803716931), criado em 2026-10-15 para o commit [`066037920817b5f616beadb7103db5b20556329d`](https://github.com/GalaxyHaze/Zith-Lang/commit/066037920817b5f616beadb7103db5b20556329d). Causa raiz não confirmada.

## Evidência

- O job `cross-builds (web, false)` compilou `libzithcLib.a` e falhou ao ligar `zithc` com `ld.lld`. Debug, Release e sanitizers passaram. O job `cross-builds (wasm-runtime, true)` foi cancelado, portanto não prova uma falha WASM independente. [Log do job](https://github.com/GalaxyHaze/Zith-Lang/actions/runs/38803716931/job/116659040794).
- O workflow fixa Zig `0.13.0`, instala LLVM 18, define o target como `x86_64-linux-musl`, exige LLVM e pede link estático (`.github/workflows/ci.yml:14, 101-145`). O CMake descreve esta configuração como Zig usando o pacote LLVM do host e liga componentes LLVM estáticos em `zithcLib` (`CMakeLists.txt:33-39, 337-390`).
- O log mostra referências indefinidas a `std::__cxx11` oriundas de membros dos archives LLVM em `/usr/lib/llvm-18/lib`, e referências a APIs LLVM com `std::__1` nos objetos de `libzithcLib.a`, incluindo `IRBuilderBase` e `TargetRegistry`.

## Interpretação Não Confirmada

Os nomes observados são compatíveis com uma incompatibilidade de ABI entre bibliotecas C++, mas não demonstram essa causa. libc++ documenta o namespace ABI `__1`; GCC documenta `std::__cxx11` na nova ABI de libstdc++. O workflow não regista a configuração efetiva de headers/runtime C++ do Zig, e as definições nos archives ainda não foram examinadas. Sem reprodução, isto permanece apenas uma pista.

## Experimento em Falta

Reexecutar o job do mesmo commit num ambiente CI equivalente, com Zig `0.13.0` e os mesmos pacotes LLVM 18. Guardar o comando final de link e a seleção efetiva de headers/bibliotecas (`zig c++ -###`); comparar referências e definições com `llvm-nm -C`; depois fazer um build de controlo com a ABI C++ alinhada à instalação LLVM. A pista só ganha suporte causal se esse controlo resolver as referências e permitir o link. Esta reprodução não foi executada localmente porque `zig` não está instalado.

Fontes primárias adicionais: [garantias ABI de libc++](https://libcxx.llvm.org/ABIGuarantees.html) e [documentação da dual ABI de libstdc++](https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_dual_abi.html).
