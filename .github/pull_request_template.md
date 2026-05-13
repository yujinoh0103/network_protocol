## 작업 요약
<!-- 이번 PR에서 변경한 내용을 간단히 적어주세요. -->


## 담당 파트
<!-- 해당되는 항목에 체크해주세요. -->
- [ ] A - Judge FSM / TURN Timer / 참가자 배열 / 탈락 판정
- [ ] B - Player FSM / 369 Engine / WAIT_ACK Timer
- [ ] C - L3 Message / Event / L2-L3 Interface / main / GitHub 관리
- [ ] 통합 테스트
- [ ] 문서화 / 제출 패키징

## 변경 파일
<!-- 주요 변경 파일을 적어주세요. -->
- 

## 구현 내용
<!-- 실제로 구현하거나 수정한 내용을 적어주세요. -->
- 
- 
- 

## 스펙 반영 사항
<!-- 최종 프로젝트 스펙 중 반영한 내용을 적어주세요. 필요 없는 항목은 지워도 됩니다. -->
- [ ] ACK는 L2에서 처리한다.
- [ ] L3 메시지는 JOIN, SETUP, TURN, ANSWER, GAMEOVER만 사용한다.
- [ ] JOIN은 dst=0으로 전송한다.
- [ ] SETUP, TURN, ANSWER, GAMEOVER는 브로드캐스트 방식으로 처리한다.
- [ ] TURN 메시지에는 현재 숫자를 포함하지 않는다.
- [ ] Player는 SETUP 수신 후 내부 숫자를 1로 초기화한다.
- [ ] Player는 TURN 흐름을 바탕으로 현재 숫자를 내부적으로 계산한다.
- [ ] Judge는 턴 수에 따라 timeout을 5초, 3초, 2초로 설정한다.
- [ ] Judge는 WRONG_ANSWER, TIMEOUT, OUT_OF_TURN 발생 시 GAMEOVER를 전송한다.
- [ ] GAMEOVER 이후 IDLE 상태로 복귀한다.

## 테스트 결과
<!-- 확인한 항목에 체크해주세요. 필요 없는 항목은 지워도 됩니다. -->
- [ ] 컴파일 확인
- [ ] 시리얼 출력 확인
- [ ] Judge 1개, Player 1개 보드 테스트
- [ ] 보드 5개 통합 테스트
- [ ] 정상 Join Phase 확인
- [ ] SETUP 이후 RUNNING 진입 확인
- [ ] TURN / ANSWER 정상 흐름 확인
- [ ] WRONG_ANSWER 처리 확인
- [ ] TIMEOUT 처리 확인
- [ ] OUT_OF_TURN 처리 확인
- [ ] GAMEOVER 이후 IDLE 복귀 확인
- [ ] 아직 테스트하지 않음

## 테스트 로그
<!-- 가능하면 시리얼 로그를 붙여주세요. -->

```txt

```

## 관련 이슈
<!-- 관련 이슈가 있으면 적어주세요. -->
<!-- 예: close #1 -->


## 리뷰어 확인 요청
<!-- 리뷰어가 특별히 확인해줬으면 하는 부분을 적어주세요. -->
- 

## 참고 사항
<!-- 미정 사항, 주의할 점, 다음 PR에서 이어서 할 내용을 적어주세요. -->