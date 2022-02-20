#include "heapTimer.h"

timeHeap* timeHeap::timeContain;

timeHeap::timeHeap():size(0){
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
    size++;
    heapInsert(heap.size()-1);                                     //插入新的定时器
}

/*
void timeHeap::delTimer(timer* _timer){                            //销毁指定定时器
    if(_timer==nullptr) return;
    if(!_timer->closedConn){
        std::cout<<"del timer"<<std::endl;
        _timer->userData->clientHttp->closeConn();
        _timer->closedConn=true;
    }
}
*/
void timeHeap::popTimer(){
    if(size==0) return ;
    if(heap[0]->userData!=nullptr) heap[0]->userData->clientHttp->closeConn();
    delete heap[0];
    size--;
    heap[0]=heap[size];
    heap[size]=nullptr;
    heap.pop_back();
    heapify(0);
}

void timeHeap::adjTimer(timer* _timer){
    time_t cur=time(nullptr);   //旧的不管，等着定时pop
    timer* _timer1=new timer(cur+3*DELAY);
    _timer->userData=nullptr;
    _timer1->userData=_timer->userData;
    addTimer(_timer1);
}
/* 心跳函数*/
void timeHeap::tick(){
    if(size==0) return;
    timer* tmp=heap[0];
    time_t cur=time(nullptr);
    while (size>0)
    {   
        if(tmp==nullptr) break;
        //std::cout<<tmp->expire<<" "<<cur<<std::endl;
        if(tmp->expire>cur) break;
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
    //std::cout<<idx<<std::endl;

void timeHeap::heapInsert(int idx){
    //heapInsert就是往上找根,根是idx
    while(heap[idx]->expire < heap[(idx-1)/2]->expire){
        std::swap(heap[idx],heap[(idx-1)/2]);
    }
}
