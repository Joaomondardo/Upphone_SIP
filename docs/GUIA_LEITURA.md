# Por onde começar

O Upphone tem duas partes. O **aplicativo** roda no Windows e faz chamadas usando uma central telefônica. O **backend** é o servidor da Upp: guarda cadastros, entrega configurações e recebe informações dos aplicativos. O painel web é uma tela desse servidor, usada para administração.

O servidor da Upp e a central telefônica têm responsabilidades diferentes. Um aplicativo pode alcançar o servidor da Upp e, mesmo assim, não conseguir registrar o ramal na central ou completar uma chamada.

## Escolha o assunto

| Quero entender… | Leia |
| --- | --- |
| Onde está cada parte do aplicativo | [Mapa do aplicativo](MAPA_MODULOS.md) |
| Onde está cada parte do servidor | [Mapa do backend](../../upp-provisionamento/docs/MAPA_MODULOS.md) |
| O caminho da ativação, configuração e atualização | [Fluxos principais](FLUXOS_PRINCIPAIS.md) |
| Qual valor prevalece quando há configurações diferentes | [Ordem das configurações](ORDEM_CONFIGURACOES.md) |
| O que aplicativo e servidor enviam um ao outro | [Comunicação com o backend](COMUNICACAO_BACKEND.md) |
| Como verificar uma alteração | [Guia de validação](GUIA_VALIDACAO.md) |
| Por que certas soluções foram escolhidas | [Decisões de arquitetura](DECISOES_ARQUITETURA.md) |

Leia um assunto por vez. Os caminhos dos arquivos e nomes das funções permitem conferir a explicação no código. Estes textos descrevem a versão examinada em 20/10/2026; não substituem o código atual nem comprovam testes reais de telefonia.

## Termos usados no projeto

| Termo | Significado aqui |
| --- | --- |
| SIP | Protocolo usado para registrar o ramal e iniciar, controlar e encerrar chamadas |
| PBX | Central telefônica que atende os ramais e encaminha chamadas |
| Provisionamento | Entrega e aplicação de configurações pelo servidor, evitando preencher tudo manualmente |
| API / rota | Forma de comunicação entre programas / endereço que recebe uma operação nessa comunicação |
| HTTP / HTTPS | Protocolo de comunicação web / sua versão protegida por TLS |
| Token | Credencial de acesso do aplicativo à API; não é a senha SIP |
| Heartbeat | Mensagem periódica com o estado informado pelo aplicativo |
| Revisão / ETag | Número de uma mudança de configuração / identificador usado para comparar o conteúdo |
| Persistência | Gravação de dados para que continuem disponíveis depois que o programa fechar |
| MFC / pjproject | Biblioteca de interface Windows / conjunto de bibliotecas de comunicação usado pelo aplicativo |
| Thread | Linha de execução; permite, por exemplo, fazer uma requisição sem travar a janela |
| Compilação (build) | Processo que transforma o código e seus recursos em executável |
| Homologação | Testes de aceitação em um ambiente representativo do cliente |

## Como escrever novas explicações

Use frases curtas e explique o termo técnico na primeira ocorrência. Mostre quem recebe o dado, o que faz com ele e para onde o envia. Nos comentários, explique o motivo de uma regra pouco óbvia, como aguardar o fim da chamada. Preserve nomes de funções e campos de comunicação para que continuem fáceis de localizar. Não traduza identificadores existentes apenas para simplificar o texto.
