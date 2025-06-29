// OrderStatus.cpp
#include "OrderStatus.h"

OrderStatus::OrderStatus() {
    // 构造链表：Created -> Paid -> Completed
    head = new StateNode(OrderState::Created);
    head->next = new StateNode(OrderState::Paid);
    head->next->next = new StateNode(OrderState::Completed);
    cur = head;
}

OrderStatus::~OrderStatus() {
    // 释放链表节点
    StateNode* ptr = head;
    while(ptr) {
        StateNode* next = ptr->next;
        delete ptr;
        ptr = next;
    }
}

OrderState OrderStatus::getState() const {
    return cur->state;
}

void OrderStatus::nextState() {
    if (cur->next) {
        cur = cur->next;
    }
}
