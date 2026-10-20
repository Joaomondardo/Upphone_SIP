# Registro de testes em laboratório — Upphone SIP 1.4.0 (provisionamento)

Registro no formato da seção 9.3 da [especificação](ESPECIFICACAO_SOFTPHONE_UPP.md). São ensaios de **laboratório** (simulador de PABX, servidor de desenvolvimento): servem para regressão e para achar defeitos, **não substituem** a homologação na central real de cada perfil de cliente nem o piloto (seções 8 e 9).

## Identificação

| Campo | Valor |
| --- | --- |
| Datas | 19 e 20/10/2026 |
| Executor | Assistente de desenvolvimento (Claude Code), a pedido da equipe Upp |
| Ambientes | Notebook de desenvolvimento Windows 10 Home 22H2 (build 19045); VM VirtualBox "UpphoneTeste" com Windows Server 2022 Standard Evaluation (Desktop Experience, build 20348), UAC ligado, usuário administrador com aprovação |
| Backend | `upp-provisionamento` em modo desenvolvimento (SQLite e PostgreSQL 17 descartáveis), heartbeat de 30 s e "sem contato" após 95 s |
| PABX | Asterisk em contêiner (imagem `andrius/asterisk`), PJSIP/UDP em 127.0.0.1:5070, ramais 1001–1003, eco no 600 e atendimento em silêncio no 700 |
| Rede | Loopback e NAT do Docker (notebook); NAT do VirtualBox (VM) |
| Transporte e codecs | UDP; PCMA/PCMU (o Asterisk de teste não tem G.729) |
| Áudio | Dispositivo padrão do Windows no notebook; VM sem entrada de áudio (de propósito). **Qualidade de áudio não avaliada** |

## Artefatos

Versão 1.4.0, MicroSIP 3.22.16 Lite, pjproject `bd69054a` (2.17-dev, TLS pelo Schannel do Windows desde 20/10 à tarde, com o patch `build/pjproject/schannel-rand-bytes.patch`), branch `provisionamento` commit `d66918b` (as fontes destes artefatos, com as correções abaixo). Não assinados (o certificado da Upp não existe ainda).

| Arquivo | SHA-256 | Bytes |
| --- | --- | --- |
| `x64/Upphone.exe` | `665803bb852c7fdf2d6ce2706c128fdd354958b35e1fe9e08d81b1a9e76d5d0f` | 7.640.576 |
| `Win32/Upphone.exe` | `0144bff904c7c1938b473aa5147561085a650541a99651d138cd08cb96cb31f4` | 6.353.408 |
| `Upphone-SIP-1.4.0-x64-setup.exe` | `18ca111c57a4f75125f8ff9baa215f8c9afad11adb6595811dcebfe0248be131` | 4.978.190 |
| `Upphone-SIP-1.4.0-Win32-setup.exe` | `a15e9351527da8c460bead68c918d0a7eada4d387f68697c348e63f4e5a8f222` | 4.561.280 |

Hashes da compilação final (20/10, com TLS, as correções até D14 e os textos de TLS e de reversão). O Win32 desta compilação foi o instalado na segunda rodada de TLS da VM (executável conferido pelo hash); a primeira rodada, em x64, usou a compilação anterior, sem os textos de "Falha no TLS da central" e "Troca de versão" (`Upphone.exe` x64 `9f746c34…0caf65`). A compilação anterior, sem TLS e com as correções até D11, tinha o `Upphone.exe` x64 `c8841bd3…02218d1` e o Win32 `3a4c5c02…677e1`. Os ensaios rodaram em compilações sucessivas do mesmo código-fonte, cada uma com as correções conhecidas até ali: a rodada da VM de 19/10 antes de D5 e D6, a rodada do notebook a partir do T13 com D5 e D6, e a rodada da VM de 20/10 com D5–D8 (D9–D11 corrigidos e conferidos nela mesma). Para testar a atualização e a reversão foi gerada uma 1.4.1 de teste do mesmo código. Qualquer nova compilação precisa de identificação própria (seção 8.1).

## Resultados por caso

| Caso | Situação no laboratório | O que foi feito e observado |
| --- | --- | --- |
| T01 | Aprovado (laboratório) | x64: instalação pelo assistente e silenciosa, atualização pelo app, reinstalação e desinstalação (pelo usuário e silenciosa), com UAC. **Cliente antigo:** 1.3.1 publicada (SHA-256 conferido com a release) com conta manual, contatos com acentos e 21 chamadas no histórico, atualizada para 1.4.0 pelo assistente sem código: nenhuma ativação pedida, conta manual, senha cifrada, contatos e histórico idênticos. **Win32:** instalação silenciosa com `/CODIGO` em `Program Files (x86)`, ativação como x86 e atualização para a 1.4.1 com o instalador de 32 bits. Sem assinatura (pendente) |
| T02 | Aprovado | Código válido (assistente, `ativacao.ini`, tela do app); dígito errado recusado no instalador; já usado, cancelado, expirado e de outro ambiente com a mensagem certa no app; 10 ativações simultâneas com o mesmo código no PostgreSQL: 1 aceita, 9 `code_used` |
| T03 | Aprovado (API) | Token copiado para outra máquina recusado (`identity_mismatch`, evento registrado); token de homologação recusado no servidor de desenvolvimento; escopo de cliente coberto pelos testes automáticos do painel e da API |
| T04 | Aprovado | Ramal 1003 com usuário de autenticação `a1003`, servidor e domínio `upp.teste` (inexistente no DNS) e proxy 127.0.0.1:5070: registro e chamada passaram pelo proxy com `INVITE sip:700@upp.teste` e `Digest username="a1003"`; registro mantido após reiniciar o app |
| T05 | Parcial | Configuração impossível para o app (TLS) recusada e última válida mantida; gravação negada e concorrência no arquivo não foram forçadas |
| T06 | Parcial | Chamadas originadas (número e `tel:700` passados ao app pela linha de comando) e recebida (atendida por `/answer`) com o Asterisk; histórico local registrou tipo e duração. Identificação conferida só no simulador |
| T07 | Parcial | PCMA negociado (primeiro do perfil). PCMU e G.729 não forçados; G.729 indisponível no simulador |
| T08–T10 | Não executado | DTMF/URA, transferências, headsets, redes reais |
| T11 | Aprovado (laboratório) | VM x64 com central SIP/TLS de teste (certificado de uma CA de teste para `pbx-tls.upp.test`) e perfil TLS com SRTP obrigatório. O app anuncia a capacidade `tls` e recebe o perfil. **CA não confiável:** conexão fechada logo após o handshake, sem REGISTER, e a barra mostra "Certificado não confiável". **CA confiável:** TLS 1.3 com SNI, REGISTER/PUBLISH por `SIP/2.0/TLS`, "Online" com o ícone seguro. **Chamadas:** INVITE por TLS com `RTP/SAVP` e `a=crypto` (AES-256 e AES-128), chaves diferentes a cada chamada e mídia, log sem "simple random generator". **Endereço fora do certificado** (IP): recusado, "Certificado não confere"; o app se recupera sozinho quando o perfil volta ao nome certo. **Central só com TLS 1.1:** recusada. **Nenhum pacote UDP** chegou à central em toda a rodada. **Win32** (segunda rodada, instalado com `/CODIGO`, ativado como x86): o mesmo registro por TLS 1.3 e as chamadas com SRTP; trocando o perfil com o app aberto, a barra passou por "Serviço indisponível" (central fora do ar), "Certificado não confiável", "Falha no TLS da central" (central só com TLS 1.1) e de volta a "Serviço indisponível". Voz não testada (áudio nulo na VM, central de teste sem mídia). Certificados HTTPS inválidos recusados nas conexões com a Upp (rodada anterior) |
| T12 | Parcial | Revisão nova durante chamada ficou pendente sem derrubar a chamada; aplicada 1 s depois do fim. Reverter pelo painel uma configuração aplicada que quebre o registro não foi ensaiado (só a recusada pelo app, no T05) |
| T13 | Aprovado | Bloqueio online (sai do registro no próximo contato) e offline (ao abrir, registro de cerca de 2 s até o primeiro contato); chamada recusada enquanto bloqueado; desbloqueio volta a registrar; desconexão apaga a configuração e o `provisioning.dat`. O painel avisa que a senha continua válida na central (sem integração) |
| T14 | Aprovado (laboratório) | Credencial copiada para outra máquina recusada (API); `provisioning.dat` copiado para outro usuário do Windows: "não pôde ser lido ... ignorado" e apagado; `ativacao.ini` com `/USUARIO` ignorado pelo outro usuário e mantido para o certo. Restauração de backup não testada |
| T15 | Parcial | Pacote alterado, versão diferente, sem SHA-256, assinatura inválida, download interrompido e redirecionamento para HTTP recusados; redirecionamento do GitHub Releases aceito; UAC recusado e aceito; distribuição gradual e janela de manutenção nos testes do backend. **Reversão autorizada** 1.4.1→1.4.0 por versão fixada no cliente, pelo app (o aviso de reversão passou a se chamar "Troca de versão", conferido na VM com a 1.3.1 fixada); **atualização obrigatória** (versão mínima): aviso ao abrir e a cada hora, "Não" não memorizado |
| T16 | Aprovado | API fora do ar ~2 min: registro no PABX mantido, "Minha conta" mostra os dois estados separados, retorno com espera crescente e eventos offline/online. PABX fora: falha 408 informada em ~80 s; registro de volta 4 s após o PABX voltar |
| T17 | Aprovado | Logs pedidos pelo painel e baixados como anexo: sem senha SIP, token, `Authorization`, `nonce` ou `response` (com e sem log SIP detalhado) |
| T18 | Parcial | Tema escuro, histórico, `tel:` tratado pelo app; no Windows 10 deste notebook outros apps (WhatsApp, Pessoas, Seu Telefone) também atendem `tel:` e o Windows decide; escala e teclado não avaliados |
| T19 | Parcial | 15 chamadas seguidas: 14 estabelecidas na janela de 4 s, memória estável (~299 MB, igual ao 1.0.0), +31 handles, log sem crescimento. Turno de 8 h não executado |
| T20 | Parcial | Build reproduzido a partir das fontes com `build.ps1`; `LICENSE.txt` e `AVISOS.txt` instalados. Pacote de fontes/SBOM não montado |

## Defeitos encontrados e corrigidos

| ID | Defeito | Correção |
| --- | --- | --- |
| D1 | Instalador assinado e depois adulterado passava como "sem assinatura" (efeito de `WTD_SAFER_FLAG`) | Flag removida; arquivo com assinatura embutida nunca é tratado como sem assinatura |
| D2 | "Não" no UAC da atualização parecia instalação concluída (o Inno Setup se eleva sozinho) | O app acompanha o processo do instalador; código 2 vira aviso e o "Sim" volta |
| D3 | Código deixado pelo instalador num computador já ativado não aparecia depois de sair da conta | A tela de ativação vem preenchida, esperando "Ativar" |
| D4 | (Processo) build "1.4.0" misturado com objetos da 1.4.1 de teste: `const.h` restaurado com data antiga | Data atualizada e recompilação completa; versões de teste devem usar uma árvore separada |
| D5 | O WinINet seguia sozinho redirecionamento de HTTPS para HTTP no download | Redirecionamentos seguidos pelo app (até 5), cada destino validado como o endereço original |
| D6 | A instância que só repassa a linha de comando (`tel:`, `/answer`) gravava e podia apagar o `provisioning.dat` e escrevia no log | `Init` só lê; gravações, exclusões e log ficam para `Start`, da instância que fica aberta |
| D7 | Versão do app com um único endereço de download para os dois instaladores: computadores de 32 bits nunca atualizariam | Endereço próprio do instalador de 32 bits (painel e migração `b20f0e9a10e5`) |
| D8 | Botão "Recarregar configuração" cortado em Minha conta | Texto "Recarregar" |
| D9 | Atualização obrigatória só reaparecia quando vencia o intervalo de busca do usuário (até um dia; nunca com "nunca") | Abaixo da versão mínima, o aviso aparece ao abrir e a cada hora, fora de chamadas |
| D10 | Parâmetros com espaço no fim (`Start-Process` do PowerShell, ferramentas da TI) não eram reconhecidos: `/minimized` virava número a discar ("Número inválido"), `/exit` não fechava | Linha de comando sem os espaços em volta antes de comparar |
| D11 | Título da janela "Atualização disponível" no aviso obrigatório | Título "Atualização necessária" |
| D12 | (Revisão, com o TLS) o MicroSIP criava o transporte TLS sem conferir o certificado da central (`verify_server` desligado): qualquer um no caminho poderia se passar pela central | Certificado conferido pela cadeia do Windows e pelo nome da central; só TLS 1.2 ou mais novo |
| D13 | (Revisão) conta configurada para TLS ou TCP se registrava em UDP, sem aviso, se esse transporte não pudesse ser criado | A conta não é registrada ("Falha na conexão") |
| D14 | (Revisão) chaves SRTP geradas com `pj_rand()` e o relógio, previsíveis: o Schannel do pjproject não implementa `pj_ssl_rand_bytes` (já era assim no build sem SSL) | Patch no pjproject fixado: `pj_ssl_rand_bytes` com `BCryptGenRandom`; aplicado pelo `build.ps1` |

## Observações e limites conhecidos

- O cliente aceita o TLS que o Windows aceitar (inclusive 1.0); o mínimo de TLS 1.2 é garantido pelo servidor (Caddy) e o instalador é sempre conferido pelo SHA-256. Política mais restrita só pelo Schannel do Windows (TI do cliente).
- Com bloqueio feito com o app fechado, o ramal registra por cerca de 2 s ao abrir, até o primeiro contato com a Upp. Para impedir chamadas de fato, a senha precisa ser trocada na central (o painel avisa).
- O pjproject deste build não grava as mensagens SIP completas no log detalhado (só avisos e estatísticas).
- Instalação silenciosa feita por outra conta ou sessão (ferramenta da TI) termina com código 5 se o Upphone estiver aberto na sessão do usuário: fechar o app antes.
- TLS da central: o certificado precisa ser de uma CA confiável no Windows (pública, ou a CA da empresa distribuída pela TI) e conter o nome usado no perfil (SubjectAltName; para IP, um SAN de IP). Certificado curinga (`*.dominio`) é recusado, como pede o RFC 5922. DTLS-SRTP não está disponível (exige OpenSSL): o SRTP usa chaves no SDP (SDES), protegidas pelo TLS; numa conta TLS o app só manda as chaves por TLS.
- O app abre uma porta TLS de escuta (como já fazia com TCP). Se alguém se conectar a ela, o Schannel cria um certificado autoassinado para essa conexão; a central normalmente reaproveita a conexão aberta pelo app.
- Central só com TLS 1.0/1.1, ou sem cifra em comum, aparece como "Falha no TLS da central"; uma central fora do ar continua como "Serviço indisponível".
- Memória privada de ~300 MB herdada do MicroSIP/pjsua (o 1.0.0 usa o mesmo); acompanhar handles no turno de 8 h.
- Ao ativar o provisionamento num computador com conta manual, a conta manual é apagada (a senha antiga não fica guardada, RF06). Sair da conta depois não a traz de volta: é preciso novo código ou a configuração manual pelo técnico.
- Uma instalação feita sem `/SERVIDOR` usa o servidor de produção: código de desenvolvimento ou homologação é recusado com "O servidor de desenvolvimento não está configurado neste computador" até a TI configurar o servidor.
- Com outro usuário do Windows já com o Upphone aberto na mesma área de trabalho, uma segunda instância repassa o comando para a primeira e sai.

## Pendente para o aceite

- Central real (SyssVoIP) por perfil: chamadas entre ramais, tronco e filas (T06–T10, FQ), G.729, DTMF, transferências, headsets e redes reais; turno de 8 h (T19).
- TLS/SRTP com a central real: certificado da SyssVoIP (CA, nome, curinga), chamadas com áudio em SRTP e o comportamento com SRTP opcional.
- Assinatura digital dos executáveis e instaladores; preencher `_GLOBAL_UPDATE_PUBLISHER`.

## Aceite

Pendente: responsável técnico Upp, responsável de telefonia e data da decisão.
