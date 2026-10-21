# Como verificar uma alteração

Escolha a verificação pelo comportamento alterado. Um teste automatizado verifica uma parte do sistema em condições controladas; uma chamada real verifica também rede, central e áudio. Registre separadamente o que foi executado e o que ainda falta.

## Escolha um ponto de partida

| Alteração | Primeira verificação | Quando ampliar |
| --- | --- | --- |
| Texto de documentação | Conferir caminhos, links, nomes e exemplos com os fontes | Quando o texto descreve um fluxo ainda não confirmado |
| Backend: configuração/ativação | `tests/test_device_api.py` e os testes de gestão relacionados | Alterações compartilhadas exigem os demais testes da API/painel |
| Backend: permissões/painel | `tests/test_management.py` e `tests/test_panel.py` | Quando mudar regra compartilhada ou modelos do banco |
| API de integração | `tests/test_integration_api.py` | Quando o contrato ou o serviço compartilhado mudar |
| C++: XML/contatos | Compilar e exercitar importação/exportação correspondente | Testar caracteres especiais, arquivo inválido e dados existentes |
| C++: configuração/SIP/áudio | Compilar e conferir aplicação/reinício/estado pendente | Testar chamadas reais e configuração da central |
| Banco | Testes relacionados e migrações em banco descartável | Mudança de estrutura exige verificar dados existentes e caminhos de migração |

## Rodar os testes do backend

Na pasta `upp-provisionamento`, use o Python do ambiente virtual preparado conforme seu README. Antes dos testes, confira `UPP_TEST_DATABASE_URL`: quando definida para PostgreSQL, a suíte apaga e recria todo o schema `public`, onde ficam as tabelas. Use somente um banco descartável. Para rodar a suíte padrão com SQLite temporário, em uma sessão de PowerShell:

```powershell
Remove-Item Env:UPP_TEST_DATABASE_URL -ErrorAction SilentlyContinue
.\.venv\Scripts\python.exe -m pytest -q tests/test_device_api.py
```

O comando Remove-Item acima remove a variável desta sessão do terminal; não apaga um arquivo. Troque o arquivo de teste conforme a tabela. Para a suíte inteira, use `-m pytest -q`. Ter a extensão Ruff instalada não significa que o executável Ruff foi instalado no ambiente virtual; confira a ferramenta antes de incluir uma chamada de terminal na rotina.

## Compilar o aplicativo

O repositório precisa estar dentro do pjproject com as dependências preparadas, conforme o README do aplicativo. A pasta de edição deste workspace, sozinha, não garante essa estrutura. No ambiente preparado e compatível:

```powershell
.\build\build.ps1 -Platforms x64 -SkipPjproject
```

`-SkipPjproject` evita recompilar essa dependência; só use quando ela já tiver sido preparada com os commits, configuração e patches exigidos. Se alterar esses itens, faça o build correspondente. Confira versão e arquitetura do executável produzido antes de testar.

## O que precisa de teste real

Receber/realizar chamadas, ouvir áudio nos dois sentidos, transferir, reconectar, negociar codecs e verificar filas dependem da central e do ambiente. Para mudanças remotas, teste recebimento durante chamada e aplicação depois do encerramento. Para atualizações, teste também o instalador distribuído e a configuração de assinatura usada.

Nos registros de teste, informe alteração, comando/caso executado, ambiente, resultado e limite da conclusão. Os roteiros detalhados e os resultados anteriores estão em `HOMOLOGACAO_LABORATORIO.md`, `PROVISIONAMENTO.md` e na especificação. Os números históricos de testes aprovados não comprovam uma execução nova.
