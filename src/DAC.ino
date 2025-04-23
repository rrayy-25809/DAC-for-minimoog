/*
 * 이 코드는 Francois Best의 Simple Synth 예제를 수정한 것입니다.
 * MIDI 입력을 받아 CV(Gate 포함)와 모듈레이션 신호로 변환하여 아날로그 장비 제어에 사용합니다.
 */

 #include <MIDI.h>            // MIDI 라이브러리
 #include "noteList.h"        // 노트 리스트 관리용 커스텀 헤더
 
 #include <MCP41HVX1.h>       // 디지털 포텐셔미터
 #include <MCP492X.h>         // 12-bit DAC
 
 // MIDI 인스턴스 생성
 MIDI_CREATE_DEFAULT_INSTANCE();
 
 // 핀 정의
 #define CS0       9   // DAC (MCP492X) 제어용 칩 선택 핀
 #define CS1       8   // (미사용, 필요시 두 번째 DAC용)
 #define CS2       7   // 디지털 포텐셔미터 (MCP41HVX1) 제어용 칩 선택 핀
 #define LDAC      2   // DAC 출력 동기화 핀
 #define WLAT      3   // DAC Write Latch 핀
 #define TrigPin   19  // Gate/Trigger 출력 핀
 
 // DIP 스위치를 통한 채널 설정 입력 핀
 #define CHAN0     14
 #define CHAN1     15
 #define CHAN2     16
 #define CHAN3     17
 
 // DAC 인스턴스
 MCP492X myDac(CS0);
 
 // 디지털 포텐셔미터 인스턴스
 MCP41HVX1 myDigipot(CS2);
 
 // 디지털 포텐셔미터 초기화 (setup에서 작동안 해서 전역에서 실행)
 const byte modZero = myDigipot.WiperSetPosition(0);
 
 // MIDI 노트 범위 설정
 const byte lowestNote = 17;                   // 가장 낮은 노트 (예: C1)
 const byte highestNote = 60 + lowestNote;     // 가장 높은 노트 (C1 + 60 = C6)
 
 // 최대 동시 발음 수 설정
 static const unsigned sMaxNumNotes = 16;
 MidiNoteList<sMaxNumNotes> midiNotes; // 현재 재생 중인 노트 관리 리스트
 
 // ─────────────────────────────────────────────
 // Trigger 상태 변경 함수
 inline void handleTrigChanged(bool inTrigActive)
 {
     digitalWrite(TrigPin, inTrigActive ? HIGH : LOW);
 }
 
 // 노트 리스트가 변경되었을 때 처리 함수
 void handleNotesChanged(bool isFirstNote = false)
 {
     handleTrigChanged(true);  // 기본적으로 Gate를 켠다
     if (midiNotes.empty())
     {
         handleTrigChanged(false);  // 노트가 없으면 Gate를 끈다
     }
     else
     {
         byte currentNote = 0;
         if (midiNotes.getHigh(currentNote))   // 가장 높은 노트를 선택 (Minimoog 방식)
         {
             // 노트를 DAC 코드로 변환 (4095: DAC 최대값)
             unsigned int noteCode = (4095 / 60) * (currentNote - lowestNote);
             myDac.analogWrite(1, noteCode);   // CV 출력 (채널 1)
 
             if (isFirstNote)
             {
                 handleTrigChanged(true);      // 첫 노트일 경우 트리거 출력
             }
         }
     }
 }
 
 // ─────────────────────────────────────────────
 // MIDI Note On 이벤트 처리
 void handleNoteOn(byte inChannel, byte inNote, byte inVelocity)
 {
     if ((inNote >= lowestNote) && (inNote <= highestNote))
     {
         const bool firstNote = midiNotes.empty();     // 첫 노트인지 확인
         midiNotes.add(MidiNote(inNote, inVelocity));  // 노트 추가
         handleNotesChanged(firstNote);                // CV/Gate 갱신
     }
 }
 
 // MIDI Note Off 이벤트 처리
 void handleNoteOff(byte inChannel, byte inNote, byte inVelocity)
 {
     if ((inNote >= lowestNote) && (inNote <= highestNote))
     {
         midiNotes.remove(inNote);      // 노트 제거
         handleNotesChanged();          // CV/Gate 갱신
     }
 }
 
 // Pitch Bend 처리 함수 (간단한 방법, 정확도 개선 여지 있음)
 void handlePitchBend(byte inChannel, byte inNote, byte inVelocity)
 {
     unsigned int pitchCode = (MIDI.getData2() << 5) | (MIDI.getData1() >> 2);
     myDac.analogWrite(0, pitchCode);   // DAC 채널 0에 Pitch Bend CV 출력
 }
 
 // 모듈레이션 휠 처리 함수 (CC #1)
 void handleModWheel(byte inChannel, byte inNote, byte inVelocity)
 {
     if (MIDI.getData1() == 1)  // CC #1: Mod Wheel
     {
         byte mod = MIDI.getData2();
         if (mod <= 63)
         {
             myDigipot.WiperSetPosition(mod);              // 낮은 값일 때 그대로 사용
         }
         else
         {
             myDigipot.WiperSetPosition(3 * mod - 126);    // 높은 값일수록 더 민감하게
         }
     }
 }
 
 // ─────────────────────────────────────────────
 // 아두이노 초기 설정 함수
 void setup()
 {
     // DAC 제어용 핀 설정
     pinMode(LDAC, OUTPUT);    digitalWrite(LDAC, LOW);
     pinMode(WLAT, OUTPUT);    digitalWrite(WLAT, LOW);
 
     // DAC 초기화 및 기본 출력값 설정
     myDac.begin();
     myDac.analogWrite(0, 2048);  // Pitch Bend 중간값
     myDac.analogWrite(1, 0);     // 노트 CV는 0으로 시작
 
     // Gate 출력 핀 설정
     pinMode(TrigPin, OUTPUT);
 
     // DIP 스위치 입력 핀 설정 (채널 설정용)
     pinMode(CHAN0, INPUT);
     pinMode(CHAN1, INPUT);
     pinMode(CHAN2, INPUT);
     pinMode(CHAN3, INPUT);
 
     // 채널 번호 계산 (0~15 → 1~16)
     byte channel = ( (digitalRead(CHAN3) << 3) |
                      (digitalRead(CHAN2) << 2) |
                      (digitalRead(CHAN1) << 1) |
                      digitalRead(CHAN0) ) + 1;
 
     // MIDI 초기화 및 콜백 함수 등록
     MIDI.begin(channel);
     MIDI.setHandleNoteOn(handleNoteOn);
     MIDI.setHandleNoteOff(handleNoteOff);
     MIDI.setHandlePitchBend(handlePitchBend);
     MIDI.setHandleControlChange(handleModWheel);
 }
 
 // ─────────────────────────────────────────────
 // 루프 함수: MIDI 신호 계속 읽기
 void loop()
 {
     MIDI.read();  // MIDI 이벤트 수신
 }
 