#include <windows.h>
#include <stdlib.h> 
#include <malloc.h>
#include <stdio.h>

#include "list.h"


void CreateList( struct Node **aNode)
{
	*aNode = malloc(sizeof(struct Node));
	(*aNode)->IPAddess=0;
	(*aNode)->NextNode =0;
	(*aNode)->PrevNode = 0;
	(*aNode)->threadID = 0;
	(*aNode)->ContinuePinging = FALSE;
}


void AddList(struct Node *List, unsigned long Ip, HANDLE threadID)
{
	struct Node *aNode;
	struct Node *aNodeNext;

//	PrintList(List);

	aNode = malloc(sizeof(struct Node));
	aNode->IPAddess = Ip;
	aNode->threadID = threadID;
	aNode->ContinuePinging = TRUE; 
		
	aNode->PrevNode = (void*)List;
	aNode->NextNode = (void *)(List->NextNode);
	if (List->NextNode)  
	{
		aNodeNext = (struct Node *)List->NextNode;
		aNodeNext->PrevNode = aNode;
	}
	List->NextNode = aNode;

//	OutputDebugString("AddList \n");
//	PrintList(List);
}

void RemoveList(struct Node *List, unsigned long Ip)
{
	struct Node *aNode;
	struct Node *aNodePrev;
	struct Node *aNodeNext;

//	PrintList(List);
	
	aNode =  List;
	if (List->NextNode) // Does the list contain nodes?
		do	// If so , start searching
		{
			aNode = (struct Node *)aNode->NextNode;
			if (aNode->IPAddess==Ip)	
			{
				aNodePrev = (struct Node *)aNode->PrevNode;
				aNodeNext = (struct Node *)aNode->NextNode;

				if (aNode->PrevNode != NULL)	aNodePrev->NextNode = aNode->NextNode;
				//OutputDebugString("Done aNodePrev->NextNode \n");

				if (aNode->NextNode != NULL)	aNodeNext->PrevNode = aNode->PrevNode;
				//OutputDebugString("Done aNodeNext->PrevNode \n");

				free( aNode);
			}
		} while(aNode->NextNode !=0);

//	PrintList(List);
}


HANDLE GetThreadID(struct Node *List, unsigned long Ip)
{
	struct Node *aNode;
	aNode =  List;
	if (List->NextNode) // Does the list contain nodes?
		do	// If so , start searching
		{
			aNode = (struct Node *)aNode->NextNode;
			if (aNode->IPAddess==Ip)	return aNode->threadID;
		} while(aNode->NextNode !=0);

//	OutputDebugString("GetThreadID Not Found \n");
	return 0;

}


BOOL GetContinuePinging(struct Node *List, unsigned long Ip)
{
	struct Node *aNode;
	aNode =  List;
	if (List->NextNode) // Does the list contain nodes?
		do	// If so , start searching
		{
			aNode = (struct Node *)aNode->NextNode;
			if (aNode->IPAddess==Ip)	return aNode->ContinuePinging ;
		} while(aNode->NextNode !=0);

//	OutputDebugString("GetContinuePinging Not Found \n");
	return FALSE;
}


void SetContinuePinging(struct Node *List, unsigned long Ip, BOOL value)
{
	struct Node *aNode;
	aNode =  List;
	if (List->NextNode) // Does the list contain nodes?
		do	// If so , start searching
		{
			aNode = (struct Node *)aNode->NextNode;
			if (aNode->IPAddess==Ip)	aNode->ContinuePinging=value;
		} while(aNode->NextNode !=0);
}


//For Debugging ...
void PrintList(struct Node *List)
{
	char strTemp[255];
	struct Node *aNode;
	aNode =  List;
	if (List->NextNode) // Does the list contain nodes?
	{
		sprintf(strTemp,"                          List ->NextNode %d \n", List->NextNode);
		OutputDebugString(strTemp);
		do	// If so , start searching
		{
			aNode = (struct Node *)aNode->NextNode;
			sprintf(strTemp,"aNode->PrevNode %d  aNode->NextNode %d  \n", aNode->PrevNode , aNode->NextNode);
			OutputDebugString(strTemp);
		} while(aNode->NextNode !=0);
	}
}
