# Upphone SIP

Softphone SIP para Windows da **Upp Tecnologia**, baseado no [MicroSIP](https://www.microsip.org/) 3.22.16 (edição Lite) e no [pjproject](https://github.com/pjsip/pjproject).

**Branch `provisionamento`: versão 1.4.0 em desenvolvimento.** Inclui cliente de ativação/configuração remota e telas de conta/áudio; a base publicada documentada é a 1.3.1. Consulte [docs/PROVISIONAMENTO.md](docs/PROVISIONAMENTO.md) para configurar o ambiente, entender as partes pendentes e planejar a homologação. O destino autorizado desta branch é o fork [Joaomondardo/Upphone_SIP](https://github.com/Joaomondardo/Upphone_SIP). O backend fica na pasta irmã `upp-provisionamento`, com destino privado autorizado [Joaomondardo/upp-provisionamento](https://github.com/Joaomondardo/upp-provisionamento); painel, integração SyssVoIP e testes reais de filas ainda não estão homologados.

- Tema claro/escuro que acompanha o Windows, com a identidade visual da Upphone
- Interface em português (Brasil), com o langpack embutido no executável
- Codecs G.711 A-law, G.711 u-law e G.729 (bcg729)
- Parâmetros padrão no código, com aplicação de perfil remoto quando provisionado; configurações técnicas gerais continuam sem a tela original
- Aviso de versão nova: uma vez por dia o app consulta a última release deste repositório no GitHub e oferece abrir a página de download (menu "Verificar por atualizações" para consultar na hora)
- Sons próprios embutidos no `.exe` (toque de chamada, chamando, segunda chamada durante outra e fim de chamada), gerados por `build/gerar_sons.py`; um `.wav` com o mesmo nome ao lado do `Upphone.exe` substitui o padrão
- Ligar ao clicar em links `tel:`, `sip:` e `callto:` (o Upphone se registra para eles ao abrir e aparece em Configurações > Aplicativos padrão)
- Sem envio de relatórios de travamento e sem prazo de validade
- Sem chat (mensagens SIP)
- Senha do ramal protegida pelo Windows (DPAPI): só o mesmo usuário no mesmo computador consegue lê-la

Histórico do projeto, decisões e pendências: [docs/HISTORICO.md](docs/HISTORICO.md).

Escopo e critérios de aceite: [especificação Markdown](docs/ESPECIFICACAO_SOFTPHONE_UPP.md) e [versão Word](docs/ESPECIFICACAO_SOFTPHONE_UPP.docx). Os documentos descrevem a base examinada 1.3.1 e o produto proposto; não são um atestado de conclusão da 1.4.0.

## Estrutura

Este repositório é a pasta do aplicativo. Para compilar, ele precisa ficar **dentro** do pjproject:

```
pjproject\                  <- https://github.com/pjsip/pjproject
  pjlib\ pjsip\ pjmedia\ ...
  third_party\
  microsip\                 <- este repositório
    build\build.ps1         <- build completo (32/64 bits + instalador)
    build\Upphone.iss       <- instalador (Inno Setup 6)
    build\pjproject\config_site.h
```

### Versões fixadas

As dependências externas ficam presas a commits exatos em [`build/pins.ps1`](build/pins.ps1). O `build.ps1` baixa e confere essas versões e se recusa a compilar com um pjproject diferente (a não ser com `-AllowOtherPjproject`).

| Dependência | Commit | Observação |
|---|---|---|
| [pjproject](https://github.com/pjsip/pjproject/commit/bd69054a696054db8fb4e62b759a3ecaf26c0189) | `bd69054a69` | master de 09/10/2026 (2.17-dev) |
| [bcg729](https://github.com/BelledonneCommunications/bcg729/commit/fb195811fe630415c9bf433c0c0bebde14314ff2) | `fb195811fe` | G.729, 14/07/2026 |
| [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp/commit/d07876c3090fa57f92af9e7fc92bb4b3f3c27390) | `d07876c309` | inclui o sqlite3, 02/10/2026 |

Para atualizar uma dependência: troque o commit em `pins.ps1`, rode o build completo, teste chamadas e só então publique.

## Pré-requisitos

- Visual Studio Build Tools 2026 com **Desenvolvimento para desktop com C++**, **ATL** e **MFC** do C++ (MSVC mais recente)
- Windows SDK 10.0.26100 ou mais recente
- Git
- Inno Setup 6, apenas para gerar o instalador

## Como compilar

```powershell
git clone https://github.com/Joao-Mondardo-Upp/Upphone_SIP.git
.\Upphone_SIP\build\setup.ps1 -Dir C:\src\upphone
C:\src\upphone\microsip\build\build.ps1 -Installer
```

O `setup.ps1` cria `C:\src\upphone` com o pjproject no commit fixado e coloca este repositório em `C:\src\upphone\microsip`, no mesmo commit do script. Use um caminho curto: o build do pjproject passa do limite de 260 caracteres do Windows em pastas muito longas.

Para compilar uma versão publicada, faça `git checkout v1.3.1` (ou outra tag) no clone antes de rodar o `setup.ps1`.

O `build.ps1`:

1. aplica `build\pjproject\config_site.h` no pjproject e confere o commit dele;
2. baixa (nas versões fixadas) e compila o **bcg729** (G.729) e o **SQLiteCpp**;
3. compila o pjproject (`Release-Static`) e o `Upphone.exe` para Win32 e x64;
4. copia o resultado para `dist\<versão>\<plataforma>\` e gera os instaladores.

Opções úteis:

| Opção | Efeito |
|---|---|
| `-Platforms x64` | compila só uma plataforma |
| `-SkipPjproject` | não recompila o pjproject (mais rápido) |
| `-AllowOtherPjproject` | aceita um pjproject fora do commit fixado (só para testes) |
| `-PfxPath arquivo.pfx -PfxPassword (Read-Host -AsSecureString)` | assina com um certificado .pfx |
| `-CertThumbprint <hash>` | assina com um certificado instalado ou em token |

## Publicar uma versão

1. Troque a versão em `const.h` (`_GLOBAL_VERSION` e `_GLOBAL_VERSION_COMMA`).
2. Crie as notas em `build\notas\v<versão>.md` (copie as da versão anterior e ajuste).
3. Compile e teste: `.\build.ps1 -Installer` (com `-PfxPath`/`-CertThumbprint` quando houver certificado).
4. Faça o commit e o `git push`.
5. Confira e publique:

```powershell
.\release.ps1 -DryRun   # só confere, não publica nada
.\release.ps1           # cria a release e a tag v<versão> no commit atual e envia os arquivos
```

O `release.ps1` se recusa a publicar com alterações não commitadas, com commits que ainda não foram para o GitHub ou com `.exe` de outra versão. Ele envia os dois instaladores, os dois portáteis em ZIP e o `SHA256SUMS.txt`. Se cair no meio, é só rodar de novo: os arquivos que já estão na release são pulados. Para autenticar, ele usa a variável `GITHUB_TOKEN` ou, se ela não existir, o login que o git já guardou para o github.com.

## Tema

O app segue a opção do Windows "Escolha o modo de aplicativo padrão" e troca de tema ao vivo. Para forçar um modo (testes ou suporte), defina a variável de ambiente `UPPHONE_THEME=light` ou `UPPHONE_THEME=dark`.

## Licença

GNU GPL v3 (veja `LICENSE`). O MicroSIP e o pjproject são GPL v2 ou posterior e o bcg729 é GPL v3, por isso o conjunto é distribuído sob a GPL v3.

Baseado no MicroSIP, Copyright (C) 2011-2026 MicroSIP (www.microsip.org).
