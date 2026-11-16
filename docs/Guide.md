# Raspberry Pi LED 문자 디바이스 드라이버

실험·실습 교안 | Device Tree Overlay 없이 전통적인 커널 모듈 + 독립적인 C Application

> 학습 핵심: `echo` 명령 대신 직접 만든 C Application에서 `open()`, `write()`, `read()`, `close()`를 이용하여 LED를 제어한다.

## 1. 학습 목표

- 커널 영역(Kernel Space)과 사용자 영역(User Space)의 역할을 구분한다.
- `module_init()`, `module_exit()`, `insmod`, `rmmod`을 설명한다.
- `register_chrdev()`와 `struct file_operations`의 기능을 이해한다.
- 응용 프로그램의 시스템 호출과 드라이버 콜백의 관계를 설명한다.
- LED ON/OFF, 상태 조회, 점멸을 C 프로그램으로 구현한다.

## 2. 장비 및 회로

| 장비 | 수량 / 조건 |
| --- | --- |
| Raspberry Pi 4/5 | 1대, Ubuntu Linux 및 해당 커널 헤더 |
| LED | 1개 |
| 전류 제한 저항 | 330 ohm 1개 |
| 브레드보드와 점퍼선 | 각 1세트 |

```text
GPIO17 (physical pin 11) --- [330 ohm] --- LED anode (+)
LED cathode (-) ------------------------- GND (physical pin 9)
```

전원을 끈 상태에서 배선한다. GPIO는 3.3 V 로직으로, GPIO 핀에 5 V를 직접 연결하지 않는다. LED의 극성을 확인한다.

## 3. 전체 시스템 구조

```text
C Application: led_app.c
  open(), write(), read(), close()
            |
            v
      /dev/rpi_ledchar
            |
            v
 Kernel: rpi_ledchar.ko
  led_open(), led_write(), led_read(), led_release()
            |
            v
   gpio_set_value_cansleep()
            |
            v
     GPIO17 -> LED
```

| Application 호출 | Driver 함수 | 의미 |
| --- | --- | --- |
| `open()` | `led_open()` | 장치 열기 |
| `write(fd,"1\n",2)` | `led_write()` | LED ON |
| `write(fd,"0\n",2)` | `led_write()` | LED OFF |
| `read()` | `led_read()` | 현재 GPIO 상태를 '0'/'1'로 읽기 |
| `close()` | `led_release()` | 장치 사용 종료 |

점멸 주기 조절은 Application의 `nanosleep()`으로 실행한다. 사용자 데이터는 `copy_from_user()`를 통해 커널로 옮기며, 읽기 응답에는 `simple_read_from_buffer()`를 사용한다.

## 4. 프로젝트 파일

| 파일 | 역할 |
| --- | --- |
| `rpi_ledchar.c` | Linux 문자 디바이스 드라이버 |
| `led_app.c` | 사용자 C 응용 프로그램 |
| `Makefile` | 모듈과 응용 프로그램 빌드 |
| `detect_gpio17.sh` | GPIO17의 전역 GPIO 번호 확인 |
| `README.md` | 설치 및 실행 안내 |

## 5. 커널 모듈 핵심

```c
module_init(led_init);
module_exit(led_exit);

static const struct file_operations led_fops = {
    .owner   = THIS_MODULE,
    .open    = led_open,
    .read    = led_read,
    .write   = led_write,
    .release = led_release,
};
```

- `led_init()`: `gpio_request()`, `gpio_direction_output()`, `register_chrdev()`, `class_create()`, `device_create()`
- `led_write()`: 사용자 문자열 0/1을 확인하고 `gpio_set_value_cansleep()`로 LED 제어
- `led_read()`: `gpio_get_value_cansleep()` 후 문자열과 파일 오프셋 처리
- `led_exit()`: `device_destroy()`, `class_destroy()`, `unregister_chrdev()`, GPIO 출력 끄기 및 `gpio_free()`

레거시 정수 GPIO API는 교육용으로 사용한다. 최신 제품용 드라이버에는 GPIO Descriptor API를 권장한다.

## 6. C Application 핵심

```c
int fd = open("/dev/rpi_ledchar", O_WRONLY);
if (fd < 0) { perror("open"); return 1; }
if (write(fd, "1\n", 2) != 2) { perror("write"); }
close(fd);
```

```c
char buf[8];
int fd = open("/dev/rpi_ledchar", O_RDONLY);
if (fd < 0) { perror("open"); return 1; }
ssize_t n = read(fd, buf, sizeof(buf) - 1);
if (n > 0) { buf[n] = '\0'; printf("LED state: %s", buf); }
close(fd);
```

Application에는 메뉴 실행과 명령줄 실행이 모두 구현되어 있다.

```text
=== LED Control Application ===
1. LED ON
2. LED OFF
3. Read LED state
4. Blink 5 times
0. Exit
Select:
```

## 7. 설치와 빌드

Ubuntu Raspberry Pi 이미지 기준:

```bash
uname -r
sudo apt update
sudo apt install -y build-essential linux-headers-raspi git
git clone https://github.com/kwonmail900/RPIDeviceDriver.git
cd RPIDeviceDriver
make
ls -l rpi_ledchar.ko led_app
```

커널 헤더는 실행 중인 커널 버전과 일치해야 한다. Pi 4/5에서 모듈은 대상 장비의 커널에 맞춰 빌드한다.

## 8. GPIO 번호 확인 및 모듈 적재

물리 핀 11은 BCM GPIO17이지만, 레거시 `gpio_request()`에 넘길 Linux 전역 GPIO 번호는 보드와 커널에 따라 달라질 수 있다.

```bash
chmod +x ./detect_gpio17.sh
GPIO_NUM=$(./detect_gpio17.sh)
printf 'GPIO_NUM=%s\n' "$GPIO_NUM"
sudo insmod ./rpi_ledchar.ko gpio_num="$GPIO_NUM"
ls -l /dev/rpi_ledchar
cat /proc/devices | grep rpi_ledchar
sudo dmesg | tail -n 15
```

스크립트가 GPIO 칩을 찾지 못하거나 여러 칩을 발견하면 `/sys/class/gpio/gpiochip*/label` 및 `/sys/class/gpio/gpiochip*/base`를 확인해 수동으로 선택한다.

## 9. Application 실행

```bash
sudo ./led_app
```

메뉴에서 1 → 3 → 2 → 3 → 4 → 0을 입력하고 LED 동작을 확인한다.

```bash
sudo ./led_app on
sudo ./led_app status
sudo ./led_app off
sudo ./led_app blink 5
```

정리:

```bash
sudo rmmod rpi_ledchar
ls -l /dev/rpi_ledchar
sudo dmesg | tail -n 15
```

## 10. 문제 해결

| 문제 | 확인 |
| --- | --- |
| `/dev/rpi_ledchar` 없음 | insmod 및 dmesg, device_create 실패 확인 |
| Permission denied | 장치 권한 확인, 실습에서는 sudo 사용 |
| Unknown symbol | 커널 버전 / API / 헤더 설정 확인 |
| gpio_request 실패(-16) | GPIO 점유 여부 확인 |
| LED 점등 실패 | 핀 11/9, 극성, 저항, 전역 번호 확인 |
| 헤더 빌드 디렉터리 없음 | 실행 커널과 일치하는 헤더 설치 |
| read()가 빈 값 반환 | read 오프셋과 EOF 확인, 다시 open |

## 11. 실습 과제

- 과제 1: `make`, `insmod`, `/dev` 장치 파일 결과 화면과 major 번호 제출
- 과제 2: Application 메뉴로 ON → 조회 → OFF → 조회 수행, LED 사진 제출
- 과제 3: `write()`와 커널 드라이버의 대응 콜백을 코드에서 찾아 설명
- 과제 4: 500 ms 점멸 간격을 200 ms로 바꿔 차이 기록
- 과제 5: `rmmod` 이후 `led_app status`의 오류 원인 설명

| 평가 항목 | 배점 |
| --- | --- |
| 배선, 빌드, 모듈 적재 | 20 |
| Application LED ON/OFF/상태 | 30 |
| Application ↔ Driver 설명 | 25 |
| 점멸 주기 변경 | 15 |
| 실습 보고서 | 10 |

## 12. 교수자 참고

- `echo`도 사용자 공간 프로그램이지만, 이 과제는 **학생이 직접 C 응용 프로그램을 만드는 것**을 목표로 한다.
- Device Tree Overlay를 추가하지 않는다는 뜻이며, Raspberry Pi 시스템의 기본 Device Tree 자체를 없앤다는 뜻은 아니다.
- Linux 6.12 헤더에서 모듈 및 응용 프로그램 컴파일을 확인했으나 Pi 4/5 실물 점등은 별도 검증이 필요하다.

참고: [Linux kernel legacy GPIO](https://www.kernel.org/doc/html/v6.10/driver-api/gpio/legacy.html), [Raspberry Pi Documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html)
