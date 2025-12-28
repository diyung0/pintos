# Pintos Operating System Projects

Pintos 운영체제 커널을 확장하여 사용자 프로그램 실행, 파일 시스템, 스레드 스케줄링, 가상 메모리를 구현한 프로젝트입니다.

## 프로젝트 개요

- **개발 기간**: 2025.09 ~ 2025.12
- **개발 환경**: Remote Linux Server (SSH), QEMU Emulator
- **언어**: C


---



## Project 1: User Program (1)

### 개발 목표
사용자 프로그램이 커널 기능을 안전하게 사용할 수 있도록 기본 시스템 콜과 인자 전달 메커니즘 구현

### 주요 구현 내용

**1. Argument Passing**
- 80x86 호출 규약에 따른 사용자 스택 구성
- 프로그램 실행 시 명령행 인자 파싱 및 스택 배치

**2. User Memory Access**
- 잘못된 메모리 접근 방지를 위한 주소 유효성 검증
- NULL 포인터, 커널 영역, 매핑되지 않은 메모리 접근 차단

**3. System Calls**
- 기본 시스템 콜: `halt()`, `exit()`, `exec()`, `wait()`
- I/O 시스템 콜: `read()`, `write()` (표준 입출력만)
- 추가 시스템 콜: `fibonacci()`, `max_of_four_int()`   


---



## Project 2: User Program (2)

### 개발 목표
파일 시스템 관련 시스템 콜 구현 및 동기화를 통한 안정적인 파일 접근 보장

### 주요 구현 내용

**1. File Descriptor**
- 프로세스별 독립적인 파일 디스크립터 테이블 (최대 128개)
- 배열 기반 O(1) 파일 접근

**2. File System Calls**
- 파일 관리: `create()`, `remove()`, `open()`, `close()`
- 파일 I/O: `read()`, `write()`, `seek()`, `tell()`
- 파일 정보: `filesize()`

**3. Synchronization**
- 전역 락을 이용한 파일 시스템 접근 동기화
- 실행 파일 보호 (file deny write)


---



## Project 3: Threads

### 개발 목표
효율적인 스레드 스케줄링과 동기화 메커니즘 구현

### 주요 구현 내용

**1. Alarm Clock**
- Busy waiting 제거
- Sleep list를 이용한 효율적인 타이머 대기

**2. Priority Scheduling**
- 우선순위 기반 스케줄링
- Priority Aging을 통한 starvation 방지

**3. Advanced Scheduler (BSD)**
- Multi-Level Feedback Queue 스케줄러
- Fixed-point 연산을 이용한 동적 우선순위 계산
- `recent_cpu`, `nice`, `load_avg` 기반 공정한 CPU 분배


---



## Project 4: Virtual Memory

### 개발 목표
가상 메모리 페이징 시스템 구현으로 물리 메모리 제약 극복

### 주요 구현 내용

**1. Supplemental Page Table**
- Hash 기반 페이지 정보 관리
- 페이지 타입별 메타데이터 (Binary, Swap, MMAP, Stack)

**2. Demand Paging & Page Fault Handler**
- Lazy loading으로 실제 접근 시점에 페이지 로드
- 페이지 폴트 시 적절한 처리 수행

**3. Frame Table & Disk Swap**
- Frame 할당 및 관리
- Second Chance (Clock) Algorithm을 이용한 페이지 교체
- Swap disk를 이용한 페이지 swap in/out

**4. Stack Growth**
- 동적 스택 확장 (최대 8MB)
- PUSHA 명령어 고려한 유효성 검사

**5. Memory Mapped Files**
- `mmap()`, `munmap()` 시스템 콜
- Lazy loading 방식의 파일-메모리 매핑


---



## 빌드 및 실행
```bash
# 프로젝트별 디렉토리로 이동
cd src/userprog  # Project 1, 2
cd src/threads   # Project 3
cd src/vm        # Project 4

# 빌드
make

# 테스트 실행
make check

# 개별 테스트 실행 예시
pintos -- run alarm-single
```


---



## 테스트 결과

- **Project 1**: 21/21 passed
- **Project 2**: All tests passed 
- **Project 3**: 22/22 passed
- **Project 4**: All tests passed

