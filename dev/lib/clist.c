#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    int data;
    struct Node* prev;
    struct Node* next;
} Node;

Node* createList() {
    Node* head = (Node*)malloc(sizeof(Node));
    if (head == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
    head->prev = NULL;
    head->next = NULL;
    return head;
}

void addNode(Node* head, int data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
    newNode->data = data;
    newNode->prev = NULL;
    newNode->next = NULL;

    Node* current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    newNode->prev = current;
    current->next = newNode;
}

void deleteNode(Node* head, Node* node) {
    if (node == NULL || head == NULL) {
        return;
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        head->next = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    free(node);
}

void printList(Node* head) {
    Node* current = head->next;  // 跳过头节点
    while (current != NULL) {
        printf("%d ", current->data);
        current = current->next;
    }
    printf("\n");
}

void freeList(Node* head) {
    Node* current = head;
    while (current != NULL) {
        Node* temp = current;
        current = current->next;
        free(temp);
    }
}

int main() {
    Node* head = createList();

    // 添加节点
    addNode(head, 1);
    addNode(head, 2);
    addNode(head, 3);
    addNode(head, 4);

    // 打印链表
    printf("Initial list: ");
    printList(head);

    // 删除节点
    Node* nodeToDelete = head->next->next;  // 删除第三个节点
    deleteNode(head, nodeToDelete);

    // 再次打印链表
    printf("List after deletion: ");
    printList(head);

    // 释放链表内存
    freeList(head);

    return 0;
}