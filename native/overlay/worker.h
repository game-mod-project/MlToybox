#pragma once

namespace mlt::ov {
// 오버레이 작업 스레드: 후킹 설치, status.json 읽기, control.json 저장, 설정·상태 파일 쓰기. 돌아오지 않는다
void runOverlayWorker(void* selfModule);
}
