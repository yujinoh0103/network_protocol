---
name: Task
about: 구현 작업, 테스트 작업, 수정 작업을 정리합니다.
title: "[Task] "
labels: ''
assignees: ''
---

## 작업 목적
<!-- 이 작업이 왜 필요한지 간단히 적어주세요. -->
<!-- 예: Judge FSM의 IDLE 상태에서 JOIN 수신 및 참가자 등록 로직을 구현한다. -->


## 담당 파트
<!-- 해당되는 항목에 체크해주세요. -->
- [ ] A - Judge FSM / TURN Timer / 참가자 배열 / 탈락 판정
- [ ] B - Player FSM / 369 Engine / WAIT_ACK Timer
- [ ] C - L3 Message / Event / L2-L3 Interface / main / GitHub 관리
- [ ] 통합 테스트
- [ ] 문서화 / 제출 패키징

## 관련 파일
<!-- 수정하거나 확인해야 하는 파일을 적어주세요. -->
- 

## 구현 항목
<!-- 구현해야 할 내용을 구체적으로 적어주세요. -->
- [ ] 
- [ ] 
- [ ] 

## 스펙 확인 사항
<!-- 최종 스펙과 맞는지 확인해야 할 내용을 적어주세요. 필요 없는 항목은 지워도 됩니다. -->
- [ ] ACK는 L2에서 처리한다.
- [ ] L3 메시지는 JOIN, SETUP, TURN, ANSWER, GAMEOVER만 사용한다.
- [ ] JOIN은 dst=0으로 전송한다.
- [ ] SETUP, TURN, ANSWER, GAMEOVER는 브로드캐스트 방식으로 처리한다.
- [ ] TURN 메시지에는 현재 숫자를 포함하지 않는다.
- [ ] Player는 자신의 차례일 때만 ANSWER를 전송한다.
- [ ] Judge는 WRONG_ANSWER, TIMEOUT, OUT_OF_TURN을 판정할 수 있어야 한다.
- [ ] GAMEOVER 이후 노드는 TURN 또는 ANSWER를 전송하지 않는다.


## 확인 항목
<!-- 완료 후 확인한 항목에 체크해주세요. 필요 없는 항목은 지워도 됩니다. -->
- [ ] 컴파일 확인
- [ ] 시리얼 출력 확인
- [ ] 보드 테스트

## 참고 사항
<!-- 공유해야 할 내용, 주의할 점, 미정 사항을 적어주세요. -->