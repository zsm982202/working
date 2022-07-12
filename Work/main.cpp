#include <iostream>
#include <vector>
using namespace std;

int partition(vector<int>& vec, int left, int right) {
	int pivot = vec[left];
	while(left < right) {
		while(left < right && vec[right] >= pivot)
			right--;
		vec[left] = vec[right];
		while(left < right && vec[left] <= pivot)
			left++;
		vec[right] = vec[left];
	}
	vec[left] = pivot;
	return left;
}

int findK(vector<int>& vec, int left, int right, int k) {
	int pivot = partition(vec, left, right);
	if(pivot == k - 1)
		return vec[pivot];
	if(pivot > k - 1)
		return findK(vec, left, pivot - 1, k);
	return findK(vec, pivot + 1, right, k);
}

//int main() {
//	vector<int> vec { 3, 7, 2, 0, -1, 9, 8, -10, 1 };
//	cout << findK(vec, 0, vec.size() - 1, (vec.size() + 1) / 2) << endl;
//	/*for(int it : vec)
//		cout << it << " ";
//	cout << endl;*/
//}

void heapUp(int arr[], int n, int k) {
	int next_k = (k - 1) / 2;
	while(next_k >= 0 && arr[next_k] < arr[k]) {
		int temp = arr[k];
		arr[k] = arr[next_k];
		arr[next_k] = temp;
		k = next_k;
		next_k = (k - 1) / 2;
	}
}


void heapDown(int arr[], int n, int k) {
	int next_k = k;
	if(2 * k + 1 < n && arr[k] < arr[2 * k + 1]) {
		int temp = arr[k];
		arr[k] = arr[2 * k + 1];
		arr[2 * k + 1] = temp;
		next_k = 2 * k + 1;
	}
	if(2 * k + 2 < n && arr[k] < arr[2 * k + 2]) {
		int temp = arr[k];
		arr[k] = arr[2 * k + 2];
		arr[2 * k + 2] = temp;
		if(next_k == k)
			next_k = 2 * k + 2;
	}
	if(next_k != k)
		heapDown(arr, n, next_k);
}

void heapDel(int arr[], int n, int k) {
	int temp = arr[k];
	arr[k] = arr[n - 1];
	arr[n - 1] = temp;
	heapDown(arr, n, k);
}

void heapCreate(int arr[], int n) {
	for(int i = n / 2 - 1; i >= 0; i--) {
		heapDown(arr, n, i);
	}
}

void heapInsert(int arr[], int n, int val) {
	arr[n] = val;
	heapUp(arr, n + 1, n);
}

//int main() {
//	/*int arr[] = { 10,8,7,7,7,2,6,1 };
//	heapDel(arr, 8, 2);*/
//	int arr[100] = { 53,17,78,9,45,65,87,32 };
//	heapCreate(arr, 8);
//	heapInsert(arr, 8, 100);
//	for(int i = 0; i < 9; i++)
//		cout << arr[i] << " ";
//	cout << endl;
//	return 0;
//}

bool match(string str, string pattern) {
	// write code here
	if(str.size() == 0 && pattern.size() == 0)
		return true;
	if(pattern.size() == 0)
		return false;
	if(pattern.size() == 1)
		return (str.size() == 1 && pattern == ".") || str == pattern;
	//cout << 1;
	if(pattern[1] == '*') {
		//cout << str << " " << pattern << endl;
		bool flag = match(str, pattern.substr(2));
		int i = 0;
		while(i < str.size() && (pattern[0] == '.' || str[i] == pattern[0])) {
			flag |= match(str.substr(i + 1), pattern.substr(2));
			i++;
		}
		return flag;
	}
	if(pattern[0] == '.' || str[0] == pattern[0])
		return match(str.substr(1), pattern.substr(1));
	return false;
}

//int main() {
//	cout << match("a", ".*") << endl;
//	return 0;
//}

#include <unordered_map>

int func(string s, int k) {
	unordered_map<char, int> map;
	int differ = 0;
	int left = 0, right = 0;
	int ret = 0;
	while(right < s.size()) {
		if((differ == k && map.count(s[right]) > 0) || differ < k) {
			if(map.count(s[right]) == 0)
				differ++;
			map[s[right++]]++;
			ret = max(ret, right - left);
		} else {
			if(map.count(s[left]) == 0)
				differ--;
			map[s[left++]]--;
		}

	}
	return ret;
}

//int main() {
//	cout << func("eceba", 3) << endl; // 3
//	cout << func("aa", 1) << endl; // 2
//	return 0;
//}

struct TreeNode {
	int val;
	TreeNode* left;
	TreeNode* right;

	TreeNode(int val) {
		this->val = val;
		left = nullptr;
		right = nullptr;
	}
};

#include <stack>

vector<int> preOrder(TreeNode* root) {
	vector<int> ret;
	stack<TreeNode*> s;
	TreeNode* temp = root;
	while(temp || !s.empty()) {
		if(temp) {
			ret.push_back(temp->val);
			s.push(temp);
			temp = temp->left;
		} else {
			temp = s.top()->right;
			s.pop();
		}
	}
	return ret;
}

vector<int> postOrder(TreeNode* root) {
	vector<int> ret;
	stack<TreeNode*> s;
	TreeNode* temp = root;
	TreeNode* visited = nullptr;
	while(temp || !s.empty()) {
		if(temp) {
			s.push(temp);
			temp = temp->left;
		} else {
			temp = s.top();
			if(temp->right && temp->right != visited) {
				temp = temp->right;
			} else {
				s.pop();
				ret.push_back(temp->val);
				visited = temp;
				temp = nullptr;
			}
		}
	}
	return ret;
}

//int main() {
//	TreeNode* root = new TreeNode(1);
//	root->left = new TreeNode(2);
//	root->right = new TreeNode(3);
//	root->left->left = new TreeNode(4);
//	root->left->right = new TreeNode(5);
//	root->left->left->left = new TreeNode(6);
//	root->right->right = new TreeNode(7);
//	vector<int> ret = postOrder(root);
//	for(int it : ret)
//		cout << it << " ";
//	cout << endl;
//	return 0;
//}

vector<int> m_n(vector<int> vec, int m) {
	vector<int> ret;
	int n = vec.size();
	int left = 0, right = m;
	while(right < n) {
		int j, v_max = INT_MIN;
		for(int i = left; i <= right; i++) {
			if(v_max < vec[i]) {
				v_max = vec[i];
				j = i;
			}
		}
		ret.push_back(vec[j]);
		left = j + 1;
		right++;
	}
	return ret;
}

//int main() {
//	vector<int> ori { 6,9,1,4,6,8,1,5,6,8,1,5,0 };
//	vector<int> ret = m_n({ 6,9,1,4,6,8,1,5,6,8,1,5,0 }, 2);
//	for(int it : ori)
//		cout << it;
//	cout << endl;
//	for(int it : ret)
//		cout << it;
//	cout << endl;
//	return 0;
//}

class Page {
public:
	int pageId;
	Page* prev;
	Page* next;

	Page(int id):pageId(id), prev(nullptr), next(nullptr){}
};

class LRU {
public:
	Page* head, * tail;
	int capacity;
	int count;
	unordered_map<int, Page*> map;

	LRU(int capacity):capacity(capacity), count(0), head(nullptr), tail(nullptr) {}

	void visit(int id) {
		if(map.count(id) > 0) {
			Page* page = map[id];
			if(page != head) {
				Page* pre = page->prev;
				pre->next = page->next;
				if(pre->next)
					pre->next->prev = pre;
				page->prev = nullptr;
				page->next = head;
				head->prev = page;
				head = page;
			}
		} else {
			if(count == capacity) {
				deletePage();
				count--;
			}
			count++;
			Page* temp = new Page(id);
			map[id] = temp;
			if(head) {
				temp->next = head;
				head->prev = temp;
				head = temp;
			} else {
				head = tail = temp;
			}
		}
	}

	void deletePage() {
		Page* temp = tail;
		tail = tail->prev;
		tail->next = nullptr;
		delete temp;
	}

	void print() {
		Page* temp = head;
		while(temp) {
			cout << temp->pageId << " ";
			temp = temp->next;
		}
		cout << endl;
	}
};

//int main() {
//	LRU a(5);
//	a.visit(1);
//	a.visit(2);
//	a.visit(3);
//	a.visit(4);
//	a.visit(5);
//	a.print();
//	a.visit(6);
//	a.print();
//	a.visit(3);
//	a.print();
//	a.visit(6);
//	a.print();
//	a.visit(8);
//	a.print();
//	return 0;
//}