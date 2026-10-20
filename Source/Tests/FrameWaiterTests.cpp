// Portable fault injection for the actual Windows wait wrapper.
// Compile with -I Source/Tests/Win32Mock; this mock is never in the game include path.
#include "../OrbitX/FrameWaiter.h"
#include <iostream>
#include <limits>
int main(){
 for(DWORD result:{WAIT_OBJECT_0,WAIT_OBJECT_0+1,WAIT_TIMEOUT}){
  Mock::Reset();{OrbitX::FrameWaiter w;Mock::result=result;w.Wait(3333333);assert(Mock::due==-33334&&Mock::lastTimeout==4&&Mock::lastCount==1&&!Mock::sleeps&&!Mock::closes);}
  assert(Mock::closes==1);
 }
 Mock::Reset();{OrbitX::FrameWaiter w;w.Wait(1);assert(Mock::due==-1&&Mock::lastTimeout==1);w.Wait(0);assert(Mock::due==-1);w.Wait(std::numeric_limits<int64_t>::min());assert(Mock::due==-1);w.Wait(std::numeric_limits<int64_t>::max());assert(Mock::due==-10000000&&Mock::lastTimeout==1000);}
 Mock::Reset();Mock::high=false;{OrbitX::FrameWaiter w;w.Wait(100);assert(Mock::normalCreates==1&&Mock::lastCount==1&&Mock::reports==1);}
 Mock::Reset();Mock::high=Mock::normal=false;{OrbitX::FrameWaiter w;w.Wait(100);assert(!Mock::arms&&Mock::lastCount==0&&Mock::reports==1);}
 for(bool armingFailure:{false,true}){
  Mock::Reset();{OrbitX::FrameWaiter w;Mock::arm=!armingFailure;Mock::result=WAIT_FAILED;w.Wait(1000);assert(Mock::closes==1&&Mock::lastCount==0&&Mock::reports==1);w.Wait(1000);assert(Mock::arms==1&&Mock::reports==1);}
  assert(Mock::closes==1);
 }
 // Unexpected return, then total message-wait failure: positive bounded Sleep,
 // no failed-handle reuse, no repeated diagnostic spam.
 Mock::Reset();Mock::result=128;Mock::fallbackResult=WAIT_FAILED;
 {OrbitX::FrameWaiter w;for(int i=0;i<20;++i)w.Wait(1000000000);assert(Mock::arms==1&&Mock::closes==1&&Mock::reports==1&&Mock::sleeps==20&&Mock::lastSleep==10);}
 std::cout<<"Actual FrameWaiter: timer/input/timeout, fallback, failure and bounded arithmetic paths passed\n";
}
