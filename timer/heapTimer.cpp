#include "heapTimer.h"
timeHeap::timeHeap(int cap):size(cap){
    heap=std::vector<timer*>(cap);
    for(int i=0;i<cap;i++){ 
        heap[i]=nullptr;
    }
}
timeHeap::~timeHeap(){
    for(int i=0;i<size;i++){
        delete heap[i];
    }
}

timer* timeHeap::top() const{
    if(heap.empty()) return nullptr;
    return heap.front();
}

void timeHeap::addTimer(timer* _timer){
    if(!_timer) return ;
    heap.emplace_back(_timer);
    heapInsert(heap.size()-1);                     //插入新的定时器
}

void timeHeap::delTimer(timer* _timer){                            //销毁指定定时器
    if(!_timer) return;
    _timer->cb_func=nullptr;
}

void timeHeap::popTimer(){
    if(heap.empty()) return ;
    heap[0]=heap[--size];
    heapify(0);
}

void timeHeap::adjTimer(timer* _timer,int delayTime){
    _timer->expire+=delayTime;
    for(int i=0;i<heap.size();i++){
        if(_timer==heap[i]){
            heapify(i);
            break;
        }
    }
}
/* 心跳函数*/
void timeHeap::tick(){
    timer* tmp=heap[0];
    time_t cur=time(nullptr);
    while (!heap.empty())
    {
        if(!tmp) break;
        if(tmp->expire>cur) break;
        if(heap[0]->cb_func){
            heap[0]->cb_func(heap[0]->userData);
        }
        popTimer();
        tmp=heap[0];
    }
}
void timeHeap::heapify(int idx){
    //heapify 就是找左，右子树，看看谁小;如果左右子树小，交换，不然就停止
    int left=2*idx+1;
    while(left<size){
        int smallest=left+1<size && heap[left+1]->expire<heap[left]->expire ? left+1:left;
        smallest = heap[smallest]->expire < heap[idx]->expire ? smallest : idx; 
        if(smallest==idx) break;
        std::swap(heap[idx],heap[smallest]);
        idx=smallest;
        left=2*idx+1;
    }
}

void timeHeap::heapInsert(int idx){
    //heapInsert就是往上找根,根是idx
    while(heap[idx]->expire < heap[(idx-1)/2]->expire){
        std::swap(heap[idx],heap[(idx-1)/2]);
    }
}
