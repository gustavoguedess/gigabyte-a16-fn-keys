# Teclas Fn do Gigabyte GAMING A16 no Linux

[English](README.md)

Driver HID-BPF que corrige as teclas Fn do teclado interno do **Gigabyte GAMING A16**
(USB `0414:8105`, "GIGABYTE USB-HID Keyboard").

**O problema:** no Linux, o teclado manda `F20` sempre que o **Fn** é apertado. O KDE, o GNOME e
o X11 tratam o F20 como *Microphone Mute*, então o Fn sozinho, ou qualquer combinação com ele,
muta o microfone. No Windows, quem resolve isso é o Gigabyte Control Center, que não existe para Linux.
Relato do problema: [Fedora Discussion](https://discussion.fedoraproject.org/t/the-fn-key-isnt-working-properly-fedora-44-workstation-gigabyte-gaming-a16-3wh/199094).

> Testado no GAMING A16 3VH, Bazzite (Fedora 44), kernel 7.2, KDE Plasma 6.7 (Wayland).
> Outros A16 com o mesmo teclado (`0414:8105`) devem funcionar. Se testar, abra uma issue contando o resultado.

## O que muda

| Tecla | Sem o driver | Com o driver |
|---|---|---|
| Fn sozinho | muta o microfone | não faz nada |
| Fn+F4 (microfone) | igual ao Fn sozinho | muta o microfone |
| Fn+F7 (ventoinha) | nada | alterna o perfil de energia ¹ |
| Fn+F8 (LCD) | `Meta+P` (troca de monitor) | desliga a tela ¹ |
| Fn+F9 (touchpad) | liga/desliga o touchpad | igual |
| Fn+F10 (avião) | desliga só o Wi-Fi | modo avião: Wi-Fi e Bluetooth ¹ |
| Fn+F11 (print) | recorte de tela (Spectacle) | igual |
| G | digita `=` | tecla livre para atalhos |
| Fn+G | nada | tecla livre para atalhos |

¹ No KDE, o `install.sh` configura esses atalhos sozinho. Em outros ambientes, veja [Outros ambientes](#outros-ambientes).

Não há controle direto da ventoinha no Linux para este modelo. O Fn+F7 troca o perfil de energia
(economia, equilibrado, desempenho), que muda o comportamento dela.

## Requisitos

- Kernel ≥ 6.12 com HID-BPF (`CONFIG_HID_BPF=y`, padrão no Fedora e no Arch; Ubuntu a partir do 25.04)
- [`udev-hid-bpf`](https://gitlab.freedesktop.org/libevdev/udev-hid-bpf) (pacote `udev-hid-bpf` no Fedora/Bazzite)
- `python3`, `rfkill` e `gdbus` (normalmente já instalados)

## Instalação

```sh
git clone <url deste repositório> gigabyte-a16-fn
cd gigabyte-a16-fn
./install.sh
```

Rode **sem sudo**: o script pede a senha só para instalar o driver. Ele:

1. instala o driver em `/etc/udev-hid-bpf/` com uma regra em `/etc/udev/rules.d/` (funciona em
   sistemas com `/usr` somente leitura, como Bazzite e Silverblue) e já o carrega, sem reiniciar;
2. instala o script do modo avião em `~/.local/bin/a16-airplane-mode`;
3. no KDE Plasma, grava os atalhos do Fn+F7, do Fn+F8 e do Fn+F10. Os atalhos que você já tem são mantidos.

Para instalar só o driver, sem mexer nos atalhos: `./install.sh --no-shortcuts`.

**Desinstalar:** `./uninstall.sh` (remove o driver, o script e os atalhos criados).

### Compilar você mesmo (opcional)

O repositório traz o driver pré-compilado em `dist/`. Para compilar: `./build.sh`. Ele usa
`clang` e `bpftool` do sistema; se não houver, compila em um container com `podman`.
O `install.sh` dá preferência ao que estiver em `build/`. Depois de recompilar, rode o `install.sh` de novo.

## Outros ambientes

O driver funciona em qualquer ambiente. Só os atalhos são do KDE. As teclas novas chegam como:

| Tecla | Tecla emitida | Nome no X11/Wayland | Sugestão |
|---|---|---|---|
| Fn+F7 | `KEY_F17` | `XF86Launch8` | alternar o perfil de energia |
| Fn+F8 | `KEY_F13` | `XF86Tools` | desligar a tela |
| Fn+F10 | `KEY_F14` | `XF86Launch5` | `~/.local/bin/a16-airplane-mode` |
| G | `KEY_F15` | `XF86Launch6` | livre |
| Fn+G | `KEY_F16` | `XF86Launch7` | livre |

No KDE, as ações ficam em **Configurações do Sistema → Teclado → Atalhos**: *Gerenciamento de energia →
Turn Off Screen / Switch Power Profile*. Ao gravar *Tools*, o KDE avisa que a tecla abre as Configurações
do Sistema; pode reatribuir.

## Como funciona

O teclado tem 5 interfaces USB. O driver (`src/0010-Gigabyte__Gaming-A16.bpf.c`) atua em duas:

- **input0 (teclado):** tira o F20 do Fn. Os atalhos do Windows que o firmware digita com o Fn apertado
  (`Meta+P`, `Ctrl+Meta+F24`…) chegam inteiros; o `Meta+P` do Fn+F8 vira F13. O `=` do G vira F15.
- **input2:** o firmware manda as teclas especiais (Fn+F4, Fn+F7, Fn+G…) por um relatório do fabricante
  que o Linux ignora. O driver acrescenta ao descritor HID um relatório com essas teclas e as injeta
  com `hid_bpf_try_input_report()`.

Detalhes, capturas e o histórico da investigação: [docs/investigation.md](docs/investigation.md) (em inglês).

### Ferramentas de diagnóstico

- `tools/capture.py`: captura guiada dos relatórios HID brutos (e do evdev, com sudo).
- `tools/watch_keys.py`: mostra as teclas que chegam (`sudo python3 tools/watch_keys.py`).
- `tools/test-load.sh`: carrega o driver de `build/` só até reiniciar e mostra as teclas.

## Projetos relacionados

- [Luminous418/Gigabyte-Gaming-A16-Linux-KB-Drivers](https://github.com/Luminous418/Gigabyte-Gaming-A16-Linux-KB-Drivers): luz RGB do teclado.
- [aasray123/gigabyte-keyboard-ctl](https://github.com/aasray123/gigabyte-keyboard-ctl)

## Licença

[GPL-2.0-only](LICENSE)
