# 🔒 Sistema de Segurança para Ativação de Máquinas Industriais

Este projeto utiliza o **RP2040** e a **placa BitDogLab** para garantir a ativação segura de máquinas industriais, impedindo o uso indevido e reduzindo o risco de acidentes. O sistema exige autenticação por senha e execução de procedimentos obrigatórios antes da ativação, garantindo que apenas usuários autorizados possam operar as máquinas.

## 🚀 Funcionalidades

- **Autenticação e Controle de Acesso**: Apenas usuários autorizados podem operar as máquinas.
- **Modo Administrador**: Permite alterar as senhas das máquinas e desbloquear o sistema em caso de bloqueio.
- **Procedimentos de Segurança**: O operador precisa executar etapas obrigatórias (pressionar botões, movimentar joystick) antes de ativar a máquina.
- **Feedback Visual e Sonoro**: LEDs RGB, buzzer e matriz de LEDs WS2812 indicam sucessos e falhas.
- **Bloqueio de Segurança**: Após três tentativas incorretas de senha, o sistema se bloqueia e exige a senha do administrador para ser destravado.
- **Interface Serial (UART)**: Todas as interações ocorrem pelo terminal serial, onde o operador insere senhas e comandos.

## 🛠️ Como Executar o Projeto

### 📥 Requisitos

- **Placa BitDogLab** com **RP2040**
- **Cabo USB** para comunicação serial
- **Software PuTTY** ou outro emulador de terminal serial
- **SDK do RP2040** configurado no **VS Code**

### ▶️ Passos para Execução

1. **Clone o repositório** ou copie os arquivos para o seu ambiente de desenvolvimento.
2. **Compile o código** utilizando o SDK do RP2040.
3. **Faça o upload do firmware** Através da extensão do Raspberry Pi Pico Project no VS Code, é possível enviar o código para a placa clicando em "Run Project (USB)". Caso não consiga enviar diretamente, copie o arquivo `.uf2` gerado para a unidade montada.
4. **Conecte-se via terminal serial** (exemplo: PuTTY) com os seguintes parâmetros:
   - Baud rate: `115200`
   - Dados: `8 bits`
   - Paridade: `Nenhuma`
   - Stop bit: `1`
5. **Aguarde a inicialização do sistema** (aproximadamente 15 segundos).
6. **Interaja com o menu exibido no terminal**, escolhendo entre:
   - `Usar Máquina A`
   - `Usar Máquina B`
   - `Modo Administrador`
7. **Siga os procedimentos obrigatórios** para ativar a máquina com segurança.

## 🎥 Explicação e Demonstração

```plaintext
https://youtu.be/BddUtNmjBbU
```

---


## 🚀 Compilação e Upload

```plaintext
1. Clone o repositório do código:
   git clone <URL_DO_REPOSITORIO>

2. Compile o programa usando CMake:
   mkdir build
   cd build
   cmake ..
   make

   Também é possível compilar diretamente na  extensão do Raspberry Pi Pico Project no VS Code clicando em "Compile Project"

3. Envie o arquivo `.uf2` gerado para o Raspberry Pi Pico:
   - Mantenha o botão `BOOTSEL` pressionado enquanto conecta o Pico ao computador.
   - Através da extensão do Raspberry Pi Pico Project no VS Code, é possível enviar o código para a placa clicando em "Run Project (USB)".
   - Caso não consiga enviar diretamente, copie o arquivo `.uf2` gerado para a unidade montada.

```