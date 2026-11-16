# Raspberry Pi LED Character Device Driver

Raspberry Pi LED 실습용 **Linux 문자 디바이스 드라이버 + C 응용 프로그램**입니다.

- Device Tree Overlay 추가 없이 전통적인 `gpio_request()`, `register_chrdev()` API 활용
- 드라이버: `rpi_ledchar.c` -> `rpi_ledchar.ko`
- 응용 프로그램: `led_app.c` -> `led_app`
- 장치 파일: `/dev/rpi_ledchar`
- 응용 프로그램에서 `open()`, `write()`, `read()`, `close()` 호출
- 지원 목표: Raspberry Pi 4/5, Ubuntu Linux (커널 헤더/레거시 API 활성화 필요)

## 배선

전원을 끈 상태에서 연결합니다.

```
GPIO17 (physical pin 11) ---- [330 ohm] ---- LED anode (+)
                                             LED cathode (-) ---- GND (physical pin 9)
```

GPIO는 3.3V 로직입니다. 5V를 직접 인가하지 마세요.

## 설치 및 실행

```bash
sudo apt update
sudo apt install -y git build-essential linux-headers-raspi

git clone https://github.com/kwonmail900/RPIDeviceDriver.git
cd RPIDeviceDriver
make

chmod +x detect_gpio17.sh
GPIO_NUM=$(./detect_gpio17.sh)
sudo insmod ./rpi_ledchar.ko gpio_num="$GPIO_NUM"

ls -l /dev/rpi_ledchar
sudo ./led_app

# 또는
sudo ./led_app on
sudo ./led_app status
sudo ./led_app off
sudo ./led_app blink 5

sudo rmmod rpi_ledchar
```

**GPIO 번호 주의:** BCM GPIO17은 리눅스 전역 번호 17이 아닐 수 있습니다. `detect_gpio17.sh`가 번호를 찾지 못하면 GPIO 칩 정보를 확인하세요.

**배포 상태:** Linux 6.12 헤더에서 드라이버와 응용 프로그램을 빌드했으며, 실제 Raspberry Pi 4/5 LED 점등은 현장 검증이 필요합니다.

## Raspberry Pi에서 wget으로 받기

```bash
wget -O RPIDeviceDriver.zip https://github.com/kwonmail900/RPIDeviceDriver/archive/refs/heads/main.zip
unzip RPIDeviceDriver.zip
cd RPIDeviceDriver-main
```

또는 `wget https://raw.githubusercontent.com/kwonmail900/RPIDeviceDriver/main/led_app.c`로 단일 파일을 받을 수 있습니다.

## 코드 구성

| File | Purpose |
|---|---|
| `rpi_ledchar.c` | Linux kernel character device driver |
| `led_app.c` | User-space C application |
| `Makefile` | Build kernel module and application |
| `detect_gpio17.sh` | Detect Linux global GPIO number for header GPIO17 |
| `README.md` | Lab instructions |

### 주의사항

이 실습은 **레거시 정수 GPIO API**를 교육 목적으로 보여줍니다. 최신 제품 개발에는 GPIO Descriptor API 권장. 기존 보드 Device Tree는 사용하나, **사용자 정의 Overlay는 작성하지 않습니다.**

소스: GPL-2.0 커널 모듈 / MIT 응용 프로그램.

## 실습교안 다운로드 (Word / PDF)

- [Word 교안 (DOCX)](docs/RPi_LED_Character_Driver_Application_Lab_Guide.docx)
- [PDF 교안](docs/RPi_LED_Character_Driver_Application_Lab_Guide.pdf)
- [교안 원고 (Markdown)](docs/Guide.md)

직접 다운로드:

```bash
wget https://raw.githubusercontent.com/kwonmail900/RPIDeviceDriver/main/docs/RPi_LED_Character_Driver_Application_Lab_Guide.docx
wget https://raw.githubusercontent.com/kwonmail900/RPIDeviceDriver/main/docs/RPi_LED_Character_Driver_Application_Lab_Guide.pdf
```

PDF/DOCX는 `docs/Guide.md`에서 자동 생성되므로 교안 내용 변경 시 원고를 수정하세요.
