// OrderStatus.h
#include <string>

enum class OrderState { Created, Paid, Completed };

struct StateNode {
    OrderState state;
    StateNode* next;
    StateNode(OrderState s) : state(s), next(nullptr) {}
};

class OrderStatus {
public:
    OrderStatus();
    ~OrderStatus();
    OrderState getState() const;
    void nextState();  // 推动订单到下一个状态
private:
    StateNode* head;   // 状态链表头（Created）
    StateNode* cur;    // 当前状态节点
};
