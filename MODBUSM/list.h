

struct Node {
	HANDLE threadID;
	unsigned long IPAddess;
	BOOL ContinuePinging;
	void *NextNode;
	void *PrevNode;
} Node ;

void CreateList(struct Node **List);
void AddList(struct Node *List,unsigned long Ip, HANDLE threadID);
void RemoveList(struct Node *List,unsigned long Ip);

HANDLE GetThreadID(struct Node *List,unsigned long Ip);
BOOL GetContinuePinging(struct Node *List, unsigned long Ip);
void SetContinuePinging(struct Node *List, unsigned long Ip, BOOL value);
void PrintList(struct Node *List);
