# Upphone SIP: descrição para o Google Stitch

Cole a **parte 1** primeiro para o Stitch gerar o conjunto de telas. Depois use os prompts da **parte 2**, um por vez, para refinar cada tela. A **parte 3** traz os textos exatos da interface.

---

## Parte 1: prompt principal

Crie o design da interface do **Upphone SIP**, o softphone da **Upp Tecnologia**, empresa brasileira de telefonia. É um aplicativo de desktop para Windows usado o dia inteiro por atendentes, recepcionistas e equipes comerciais para fazer e receber ligações pelo ramal da empresa (telefonia IP/SIP). Toda a interface em **português do Brasil**.

**Formato:** janela pequena e estreita, em pé, parecida com um telefone: cerca de 260 px de largura por 440 px de altura, redimensionável. Fica aberta num canto da tela, sempre à mão, e pode ficar "sempre no topo". O app também vive na bandeja do Windows.

**Identidade visual:**

- cor de destaque: laranja Upphone **#E46737** (hover **#F08054**), usada no botão Chamar, no botão Atender, em teclas pressionadas, em botões ligados (Espera, Mudo, Não Perturbe) e na aba ativa;
- vermelho **#FF3B30** para Terminar chamada e Recusar;
- texto branco sobre laranja e sobre vermelho;
- o app acompanha o tema do Windows, então **desenhe cada tela nos temas claro e escuro**:

| Papel | Claro | Escuro |
| --- | --- | --- |
| Fundo | #FFFFFF | #000000 |
| Superfície (teclas, campos, cartões) | #F0F0F0 | #1A1A1A |
| Superfície em hover | #E1E1E1 | #2C2C2C |
| Bordas | #CCCCCC | #3A3A3A |
| Texto | #1A1A1A | #F0F0F0 |
| Texto secundário | #6E6E6E | #969696 |

- estilo limpo e plano, cantos arredondados de 6 px, bordas finas de 1 px, sem sombras pesadas e sem gradientes;
- fonte Segoe UI (a do Windows); números do teclado grandes e legíveis;
- ícones simples de linha (telefone, microfone, alto-falante, pausa/espera, transferir, lupa, estrela, menu).

**Estrutura da janela principal:**

- **No topo:** três abas, **Telefone**, **Registros** e **Contatos**, e um botão de menu (☰) no canto direito.
- **Embaixo:** uma barra de status fina. À esquerda, um ícone colorido com o estado da conta (registrado, conectando, erro); à direita, o ramal (por exemplo "5001"). Durante uma ligação, a barra mostra "Conectado 00:42"; com a chamada em espera, mostra "Espera 00:10 / 00:42".

**Telas que preciso:**

1. **Telefone (discador), sem ligação:**
   - campo de número no topo, com lista suspensa dos últimos números discados;
   - teclado numérico 3 × 4 (1, 2, 3 / 4, 5, 6 / 7, 8, 9 / \*, 0, #), com o dígito grande e as letras pequenas em cinza (ABC, DEF...);
   - uma linha extra com **R** (rediscar), **+** e **C** (limpar);
   - botão **Chamar** laranja ocupando toda a largura;
   - dois controles de volume, cada um com um ícone que também serve para silenciar, um "−", um slider e um "+": alto-falante ("Volume de recepção") e microfone ("Volume do Microfone");
   - no rodapé, dois botões liga/desliga pequenos: **DND** (Não Perturbe) e **AA** (Atendimento automático).
2. **Telefone durante uma ligação:**
   - o campo de número mostra quem está na linha, e o teclado continua disponível para digitar opções de menus de atendimento (DTMF);
   - no lugar de Chamar, uma fileira com três botões: **Espera** (ícone, quadrado, fica laranja quando ligado), **Terminar chamada** (vermelho, largo, no meio) e **Transferir** (ícone, quadrado);
   - o ícone de microfone, quando ligado, silencia o microfone (Mudo) e fica laranja.
3. **Chamada recebida:** janela pop-up pequena, que aparece por cima de tudo, com o título "Chamada Recebida".
   - nome de quem liga em destaque, grande e centralizado, e o número ou endereço SIP abaixo;
   - dois botões lado a lado: **Atender** (laranja) e **Recusar** (vermelho);
   - um botão pequeno de **Transferir**, e o texto discreto "feche a janela para ignorar a chamada".
4. **Transferir chamada:** janela pequena.
   - campo de número com sugestões da agenda, um teclado numérico compacto no mesmo estilo do discador, e os botões **OK** e **Cancelar**;
   - opções **Transferência Cega** e **Transferência Assistida**.
5. **Registros (histórico de chamadas):**
   - tabela com as colunas **Nome**, **Número**, **Horário**, **Duração** e **Informação**;
   - um ícone em cada linha para chamada recebida, feita ou perdida (perdida em vermelho);
   - cabeçalho com seta de ordenação na coluna ordenada;
   - campo de busca com lupa no rodapé ("Procurar");
   - menu do botão direito: Chamar, Adicionar contato, Copiar, Remover, Exportar e Importar.
6. **Contatos (agenda):**
   - tabela com **Nome**, **Número** e **Informação**, com estrela de favorito e um indicador de presença (verde = livre, vermelho = em ligação, cinza = desconhecido);
   - busca com lupa no rodapé;
   - menu do botão direito: Chamar, Capturar chamada, Adicionar, Editar, Copiar, Remover, Exportar e Importar.
7. **Contato (adicionar ou editar):**
   - formulário com ícone de pessoa, campos **Nome** e **Número**, caixas "Favorito" e "Inscrição de Presença";
   - depois **Primeiro Nome**, **Último Nome**, **Número de Telefone**, **Número de Celular**, **Endereço de e-mail**, **Endereço**, **Cidade**, **Estado**, **CEP** e **Comentário**;
   - botões **OK** e **Cancelar**.
8. **Conta (configurar o ramal):** formulário curto e simples.
   - campos **Nome da Conta**, **Servidor SIP** e **Senha**, com o link "exibir senha";
   - botões **Salvar** e **Cancelar**.
9. **Atalhos (discagem rápida):**
   - grade de até 8 linhas, cada uma com **Etiqueta**, **Número**, **Tipo** (lista), **Alterar** (caixa) e **BLF** (caixa: mostrar se o ramal está livre ou ocupado);
   - caixas "Habilitar" e "Posição inferior";
   - botões **Salvar** e **Cancelar**.
   - Quando habilitados, os atalhos aparecem na janela principal como botões com nome e uma bolinha de presença.
10. **Menu principal (botão ☰):** lista suspensa com:
    - a conta ativa marcada e "Editar Conta (Ctrl+M)";
    - "Adicionar Conta..." e "Atalhos (Ctrl+S)";
    - "Sempre no topo" (com marca);
    - "Exportar/Importar", com submenu de configuração, registros de chamadas e contatos;
    - "Verificar por atualizações" e "Visitar Website (Ctrl+W)";
    - "Ajuda", com a versão (v1.3.0), e "Sair (Ctrl+Q)".
11. **Aviso de atualização:** caixa de diálogo "Atualização disponível".
    - texto "A versão 1.4.0 está disponível. Você está usando a versão 1.3.0.";
    - uma lista "Novidades:" com 3 ou 4 itens;
    - a pergunta "Deseja baixar agora?" e os botões **Sim** e **Não**.

**O que não existe no app (não desenhar):** chat ou mensagens, chamada de vídeo, gravação de chamadas e conferência pelo discador.

**Prioridades de usabilidade:**

- ligar em dois cliques;
- atender sem procurar o botão;
- estado da ligação e cronômetro sempre visíveis;
- botões grandes o bastante para uso com o mouse o dia todo;
- bom contraste nos dois temas;
- tudo legível em telas com escala de 100% a 150%.

---

## Parte 2: prompts para refinar cada tela

### Discador

> Refine a aba Telefone do Upphone SIP. O teclado é o centro da tela: teclas grandes, com superfície #F0F0F0 (escuro #1A1A1A), canto de 6 px e borda de 1 px; ao passar o mouse, a borda fica laranja #E46737, e ao pressionar a tecla fica laranja com o dígito branco. O botão Chamar laranja ocupa a largura toda. Mostre também o estado "em ligação": a mesma tela com Espera (ligado, laranja), Terminar chamada (vermelho #FF3B30) e Transferir, e a barra de status com "Conectado 01:23".

### Chamada recebida

> Desenhe a janela "Chamada Recebida" do Upphone SIP, um pop-up de cerca de 280 × 200 px que precisa chamar atenção sem assustar. Nome de quem liga grande ("Maria Souza"), número abaixo ("+55 48 3333-2222"), botões Atender (laranja #E46737) e Recusar (vermelho #FF3B30) lado a lado, texto discreto "feche a janela para ignorar a chamada". Versões clara e escura.

### Registros e Contatos

> Desenhe as abas Registros e Contatos do Upphone SIP numa janela estreita (260 px). Tabela densa, mas legível, com cabeçalho de colunas e seta de ordenação, ícones de chamada recebida, feita e perdida (perdida em vermelho), estrela de favorito e bolinha de presença nos contatos. Campo "Procurar" com lupa no rodapé. Mostre também o menu do botão direito aberto.

### Conta e Atalhos

> Desenhe as janelas "Conta" (Nome da Conta, Servidor SIP, Senha com "exibir senha", botões Salvar e Cancelar) e "Atalhos" (grade de 8 linhas com Etiqueta, Número, Tipo, Alterar e BLF; caixas Habilitar e Posição inferior) do Upphone SIP, no estilo de janela de configuração do Windows 11, com a cor de destaque laranja #E46737 no botão principal.

---

## Parte 3: textos exatos da interface (pt-BR)

| Onde | Textos |
| --- | --- |
| Abas | Telefone · Registros · Contatos |
| Discador | Chamar · Terminar chamada · Espera · Transferir · Rediscar · Limpar · Volume de recepção · Volume do Microfone · Silenciar recepção · Silenciar transmissão · DND (Não Perturbe) · AA (Atendimento automático) |
| Barra de status | Conectado 00:42 · Espera 00:10 / 00:42 · Chamada ativa · Chamada em espera · Chamada colocada em espera pelo outro lado |
| Chamada recebida | Chamada Recebida · Atender · Recusar · feche a janela para ignorar a chamada |
| Transferência | Transferência de chamada · Transferência Cega · Transferência Assistida · OK · Cancelar |
| Listas | Nome · Número · Horário · Duração · Informação · Procurar |
| Menus das listas | Chamar · Capturar chamada · Adicionar · Adicionar contato · Editar · Copiar · Remover · Exportar · Importar |
| Contato | Contato · Favorito · Inscrição de Presença · Primeiro Nome · Último Nome · Número de Telefone · Número de Celular · Endereço de e-mail · Endereço · Cidade · Estado · CEP · Comentário |
| Conta | Conta · Nome da Conta · Servidor SIP · Senha · exibir senha · Salvar · Cancelar |
| Atalhos | Atalhos · Etiqueta · Número · Tipo · Alterar · BLF · Habilitar · Posição inferior |
| Menu principal | Editar Conta · Adicionar Conta... · Tornar Ativo · Atalhos · Sempre no topo · Exportar/Importar · Verificar por atualizações · Visitar Website · Ajuda · Sair |
| Atualização | Atualização disponível · A versão %s está disponível. Você está usando a versão %s. · Novidades: · Deseja baixar agora? |
