#include "../include/bpt.h"

#include <stdlib.h>

#include <list>
#include <algorithm>
using std::swap;
using std::binary_search;
using std::lower_bound;
using std::upper_bound;

namespace bpt {

	/* custom compare operator for STL algorithms */
	OPERATOR_KEYCMP(index_t);
	OPERATOR_KEYCMP(record_t);

	/* helper iterating function */
	template<class T>
	inline typename T::child_t begin(T& node) {
		return node.children;
	}
	template<class T>
	inline typename T::child_t end(T& node) {
		return node.children + node.n;
	}

	/* helper searching function */
	inline index_t* find(internal_node_t& node, const key_t& key) {
		return upper_bound(begin(node), end(node) - 1, key);
	}
	inline record_t* find(leaf_node_t& node, const key_t& key) {
		return lower_bound(begin(node), end(node), key);
	}

	bplus_tree::bplus_tree(const char* p, bool force_empty)
		: fp(NULL), fp_level(0) {
		bzero(path, sizeof(path));
		strcpy(path, p);

		if(!force_empty)
			// read tree from file
			if(map(&meta, OFFSET_META) != 0)  //把db.bin中的meta信息读到meta中，=0表示读到信息，不清空
				force_empty = true;

		if(force_empty) { //清空
			open_file("w+"); // 清空可读写
			// create empty tree if file doesn't exist
			init_from_empty();
			close_file();
		}
	}

	//查找键值为key的叶子结点记录，并把表记录data赋值给value，返回0，没找到返回-1
	int bplus_tree::search(const key_t& key, value_t* value) const {
		leaf_node_t leaf;
		map(&leaf, search_leaf(key)); //找到的叶子结点的偏移，再赋值给leaf
		record_t* record = find(leaf, key); //在叶子结点找到记录
		if(record != leaf.children + leaf.n) {
			*value = record->value;
			return keycmp(record->key, key); //record->key==key返回0且应该返回0
		} else { //没找到
			return -1;
		}
	}

	//查找key=left到right的记录保存到values中，max是查找的最大数量
	//如果达到最大数量则next=true，left指向查找到value的下一个记录的地址，否则next=false
	//返回查找到记录的数量
	int bplus_tree::search_range(key_t* left, const key_t& right,
		value_t* values, size_t max, bool* next) const {
		if(left == NULL || keycmp(*left, right) > 0) //left的key>right的key，不正常
			return -1;

		off_t off_left = search_leaf(*left); //left对应的偏移量
		off_t off_right = search_leaf(right); //right对应的偏移量
		off_t off = off_left;
		size_t i = 0;
		record_t* b, * e;

		leaf_node_t leaf;
		while(off != off_right && off != 0 && i < max) {
			map(&leaf, off); //通过off赋值给leaf
			if(off_left == off) //第一次进循环时找到key在该结点所在位置
				b = find(leaf, *left);
			else
				b = begin(leaf); //除了第一次循环，从叶子结点的第一个key开始复制
			e = leaf.children + leaf.n;
			for(; b != e && i < max; ++b, ++i)
				values[i] = b->value;
			off = leaf.next; //下一个叶子结点
		}

		//最后一个叶子结点只查找到right
		if(i < max) {
			map(&leaf, off_right);

			b = find(leaf, *left);
			e = upper_bound(begin(leaf), end(leaf), right);
			for(; b != e && i < max; ++b, ++i)
				values[i] = b->value;
		}

		//next=true表示还没有全部查找到，因为已经达到了最大查找数量max，并指定left为查找到value的下一个记录的地址
		if(next != NULL) {
			if(i == max && b != e) {
				*next = true;
				*left = b->key;
			} else {
				*next = false;
			}
		}

		return i;
	}

	//删除key，成功返回0失败返回-1
	int bplus_tree::remove(const key_t& key) {
		internal_node_t parent;
		leaf_node_t leaf;

		//找到叶子结点的父亲
		off_t parent_off = search_index(key);
		map(&parent, parent_off);

		//找到叶子结点leaf
		index_t* where = find(parent, key);
		off_t offset = where->child;
		map(&leaf, offset);

		//如果key不在叶子结点中返回-1
		if(!binary_search(begin(leaf), end(leaf), key))
			return -1;

		size_t min_n = meta.leaf_node_num == 1 ? 0 : meta.order / 2; //meta.leaf_node_num == 1表示仅有根结点
		assert(leaf.n >= min_n && leaf.n <= meta.order);

		//删除key
		record_t* to_delete = find(leaf, key);
		std::copy(to_delete + 1, end(leaf), to_delete);
		leaf.n--;

		//叶子结点小于B+树的阶/2
		if(leaf.n < min_n) {
			bool borrowed = false;
			//尝试向左兄弟借
			if(leaf.prev != 0)
				borrowed = borrow_key(false, leaf);

			//尝试向右兄弟借
			if(!borrowed && leaf.next != 0)
				borrowed = borrow_key(true, leaf);

			//没借到只能合并
			if(!borrowed) {
				assert(leaf.next != 0 || leaf.prev != 0);

				key_t index_key;

				if(where == end(parent) - 1) {
					//叶子结点的key在父亲中是最后一个，则与左边的兄弟合并
					assert(leaf.prev != 0);
					leaf_node_t prev;
					map(&prev, leaf.prev);
					index_key = begin(prev)->key;

					merge_leafs(&prev, &leaf);
					node_remove(&prev, &leaf);
					unmap(&prev, leaf.prev);
				} else {
					//与右边的兄弟合并
					assert(leaf.next != 0);
					leaf_node_t next;
					map(&next, leaf.next);
					index_key = begin(leaf)->key;

					merge_leafs(&leaf, &next);
					node_remove(&leaf, &next);
					unmap(&leaf, offset);
				}

				//移除父亲中的key
				remove_from_index(parent_off, parent, index_key);
			} else {
				unmap(&leaf, offset);
			}
		} else {
			unmap(&leaf, offset);
		}

		return 0;
	}

	int bplus_tree::insert(const key_t& key, value_t value) {
		off_t parent = search_index(key); //找到key所在叶子结点
		off_t offset = search_leaf(parent, key); //找到其键值为key对应的孩子结点偏移量
		leaf_node_t leaf;
		map(&leaf, offset); //key所在叶子结点key对应的孩子结点赋值给leaf

		if(binary_search(begin(leaf), end(leaf), key)) //如果找到key则插入失败返回1
			return 1;

		if(leaf.n == meta.order) { //叶子结点key数量=B+树的阶，满了
			leaf_node_t new_leaf; //新建一个兄弟叶子
			node_create(offset, &leaf, &new_leaf); //将new_leaf插入到leaf同一父亲结点下的后面

			size_t point = leaf.n / 2;
			//place_right=true表示把key放new_leaf里，否则放leaf里
			bool place_right = keycmp(key, leaf.children[point].key) > 0; // key比leaf中心的key长或者大
			if(place_right)
				++point;

			//把leaf里一半的key拷贝到new_leaf中
			std::copy(leaf.children + point, leaf.children + leaf.n,
				new_leaf.children);
			new_leaf.n = leaf.n - point;
			leaf.n = point;

			//判断key和value应该插入在leaf还是new_leaf里
			if(place_right)
				insert_record_no_split(&new_leaf, key, value);
			else
				insert_record_no_split(&leaf, key, value);

			unmap(&leaf, offset); //将leaf写入db.bin中
			unmap(&new_leaf, leaf.next); //将new_leaf写入db.bin中

			//在parent中插入new_leaf.children[0].key
			insert_key_to_index(parent, new_leaf.children[0].key,
				offset, leaf.next);
		} else { //叶子结点key数量<B+树的阶，没满，直接插入key和value
			insert_record_no_split(&leaf, key, value);
			unmap(&leaf, offset);
		}

		return 0;
	}

	//跟新key对应的记录，跟新成功返回0，没找到返回-1，出错返回1
	int bplus_tree::update(const key_t& key, value_t value) {
		off_t offset = search_leaf(key);
		leaf_node_t leaf;
		map(&leaf, offset);

		record_t* record = find(leaf, key);
		if(record != leaf.children + leaf.n)
			if(keycmp(key, record->key) == 0) {
				record->value = value;
				unmap(&leaf, offset);
				return 0;
			} else {
				return 1;
			} else
				return -1;
	}

	void bplus_tree::remove_from_index(off_t offset, internal_node_t& node,
		const key_t& key) {
		size_t min_n = meta.root_offset == offset ? 1 : meta.order / 2;
		assert(node.n >= min_n && node.n <= meta.order);

		// remove key
		key_t index_key = begin(node)->key;
		index_t* to_delete = find(node, key);
		if(to_delete != end(node)) {
			(to_delete + 1)->child = to_delete->child;
			std::copy(to_delete + 1, end(node), to_delete);
		}
		node.n--;

		// remove to only one key
		if(node.n == 1 && meta.root_offset == offset &&
			meta.internal_node_num != 1) {
			unalloc(&node, meta.root_offset);
			meta.height--;
			meta.root_offset = node.children[0].child;
			unmap(&meta, OFFSET_META);
			return;
		}

		// merge or borrow
		if(node.n < min_n) {
			internal_node_t parent;
			map(&parent, node.parent);

			// first borrow from left
			bool borrowed = false;
			if(offset != begin(parent)->child)
				borrowed = borrow_key(false, node, offset);

			// then borrow from right
			if(!borrowed && offset != (end(parent) - 1)->child)
				borrowed = borrow_key(true, node, offset);

			// finally we merge
			if(!borrowed) {
				assert(node.next != 0 || node.prev != 0);

				if(offset == (end(parent) - 1)->child) {
					// if leaf is last element then merge | prev | leaf |
					assert(node.prev != 0);
					internal_node_t prev;
					map(&prev, node.prev);

					// merge
					index_t* where = find(parent, begin(prev)->key);
					reset_index_children_parent(begin(node), end(node), node.prev);
					merge_keys(where, prev, node);
					unmap(&prev, node.prev);
				} else {
					// else merge | leaf | next |
					assert(node.next != 0);
					internal_node_t next;
					map(&next, node.next);

					// merge
					index_t* where = find(parent, index_key);
					reset_index_children_parent(begin(next), end(next), offset);
					merge_keys(where, node, next);
					unmap(&node, offset);
				}

				// remove parent's key
				remove_from_index(node.parent, parent, index_key);
			} else {
				unmap(&node, offset);
			}
		} else {
			unmap(&node, offset);
		}
	}

	//索引结点向兄弟结点借关键字，offset是borrower的父亲，返回true借成功，返回false借失败
	bool bplus_tree::borrow_key(bool from_right, internal_node_t& borrower,
		off_t offset) {
		typedef typename internal_node_t::child_t child_t;

		off_t lender_off = from_right ? borrower.next : borrower.prev;
		internal_node_t lender;
		map(&lender, lender_off);

		assert(lender.n >= meta.order / 2);
		if(lender.n != meta.order / 2) {
			child_t where_to_lend, where_to_put;

			internal_node_t parent;

			//从右边借
			if(from_right) {
				where_to_lend = begin(lender); //借的位置
				where_to_put = end(borrower); //放的位置

				map(&parent, borrower.parent);
				child_t where = lower_bound(begin(parent), end(parent) - 1,
					(end(borrower) - 1)->key); //父结点中找到要借
				//修改父结点中关键字从原结点最后一个key为借到的key
				where->key = where_to_lend->key;
				unmap(&parent, borrower.parent);
			} else {
				where_to_lend = end(lender) - 1;
				where_to_put = begin(borrower);

				map(&parent, lender.parent);
				child_t where = find(parent, begin(lender)->key);
				where_to_put->key = where->key;
				where->key = (where_to_lend - 1)->key;
				unmap(&parent, lender.parent);
			}

			//结点放入借到的结点
			std::copy_backward(where_to_put, end(borrower), end(borrower) + 1);
			*where_to_put = *where_to_lend;
			borrower.n++;

			//兄弟结点删掉借出的结点
			reset_index_children_parent(where_to_lend, where_to_lend + 1, offset); //将where_to_lend的父结点设置为offset
			std::copy(where_to_lend + 1, end(lender), where_to_lend);
			lender.n--;
			unmap(&lender, lender_off);
			return true;
		}

		return false;
	}

	//叶子结点向兄弟结点借关键字，offset是borrower的父亲，返回true借成功，返回false借失败
	bool bplus_tree::borrow_key(bool from_right, leaf_node_t& borrower) {
		off_t lender_off = from_right ? borrower.next : borrower.prev;
		leaf_node_t lender;
		map(&lender, lender_off);

		assert(lender.n >= meta.order / 2);
		if(lender.n != meta.order / 2) {
			typename leaf_node_t::child_t where_to_lend, where_to_put;

			if(from_right) {
				where_to_lend = begin(lender);
				where_to_put = end(borrower);
				change_parent_child(borrower.parent, begin(borrower)->key,
					lender.children[1].key);
			} else {
				where_to_lend = end(lender) - 1;
				where_to_put = begin(borrower);
				change_parent_child(lender.parent, begin(lender)->key,
					where_to_lend->key);
			}

			std::copy_backward(where_to_put, end(borrower), end(borrower) + 1);
			*where_to_put = *where_to_lend;
			borrower.n++;

			std::copy(where_to_lend + 1, end(lender), where_to_lend);
			lender.n--;
			unmap(&lender, lender_off);
			return true;
		}

		return false;
	}

	//修改偏移量为parent的结点中关键字o->n
	void bplus_tree::change_parent_child(off_t parent, const key_t& o,
		const key_t& n) {
		internal_node_t node;
		map(&node, parent);

		index_t* w = find(node, o); //找到parent中关键字为o
		assert(w != node.children + node.n);

		w->key = n; //关键字更改为n
		unmap(&node, parent);
		if(w == node.children + node.n - 1) { //关键字是parent中最后一个，则需要递归修改
			change_parent_child(node.parent, o, n);
		}
	}

	//将叶子结点right合并到left
	void bplus_tree::merge_leafs(leaf_node_t* left, leaf_node_t* right) {
		std::copy(begin(*right), end(*right), end(*left));
		left->n += right->n;
	}

	void bplus_tree::merge_keys(index_t* where,
		internal_node_t& node, internal_node_t& next) {
		//(end(node) - 1)->key = where->key;
		//where->key = (end(next) - 1)->key;
		std::copy(begin(next), end(next), end(node));
		node.n += next.n;
		node_remove(&node, &next); //删除next结点
	}

	//将键值为key记录为value插入到叶子结点leaf中
	void bplus_tree::insert_record_no_split(leaf_node_t* leaf,
		const key_t& key, const value_t& value) {
		record_t* where = upper_bound(begin(*leaf), end(*leaf), key);
		std::copy_backward(where, end(*leaf), end(*leaf) + 1); //将key对应的记录全部后移，给待插入的value腾出位置

		where->key = key;
		where->value = value;
		leaf->n++;
	}

	//insert_key_to_index(parent, new_leaf.children[0].key,	offset, leaf.next)
	//将关键字key插入offset中，old是分裂前的孩子结点，after是分裂后新的结点
	void bplus_tree::insert_key_to_index(off_t offset, const key_t& key,
		off_t old, off_t after) {
		if(offset == 0) {
			//创建新的根结点
			internal_node_t root;
			root.next = root.prev = root.parent = 0;
			meta.root_offset = alloc(&root);
			meta.height++;

			//插入old和after
			root.n = 2;
			root.children[0].key = key;
			root.children[0].child = old;
			root.children[1].child = after;

			unmap(&meta, OFFSET_META);
			unmap(&root, meta.root_offset);

			//begin(root)是root孩子的起点
			//设置meta.root_offset为root孩子的父亲
			reset_index_children_parent(begin(root), end(root),
				meta.root_offset);
			return;
		}

		internal_node_t node;
		map(&node, offset); //node是父结点
		assert(node.n <= meta.order);

		if(node.n == meta.order) { //node满了
			//新建一个node的兄弟结点
			internal_node_t new_node;
			node_create(offset, &node, &new_node);

			//找到中间位置
			size_t point = (node.n - 1) / 2;
			//place_right=true则把key放在new_node中，否则放在node中
			bool place_right = keycmp(key, node.children[point].key) > 0;
			if(place_right)
				++point;

			// prevent the `key` being the right `middle_key`
			// example: insert 48 into |42|45| 6|  |
			if(place_right && keycmp(key, node.children[point].key) < 0)
				point--;

			key_t middle_key = node.children[point].key; //找到中间位置的key

			//把middle_key后面的key移到兄弟结点new_node中
			std::copy(begin(node) + point + 1, end(node), begin(new_node));
			new_node.n = node.n - point - 1;
			node.n = point + 1;

			// put the new key
			if(place_right)
				insert_key_to_index_no_split(new_node, key, after); //将兄弟结点第一个孩子的key
			else
				insert_key_to_index_no_split(node, key, after);

			unmap(&node, offset);
			unmap(&new_node, node.next);

			//孩子结点的父亲
			reset_index_children_parent(begin(new_node), end(new_node), node.next);

			//node里关键字满了在拆分索引结点后需要进一步在node.parent中插入middle_key
			insert_key_to_index(node.parent, middle_key, offset, node.next);
		} else {
			insert_key_to_index_no_split(node, key, after);
			unmap(&node, offset);
		}
	}

	//将键值为key孩子偏移量为value插入到内部结点node中
	void bplus_tree::insert_key_to_index_no_split(internal_node_t& node,
		const key_t& key, off_t value) {
		//找到key应该在node中的位置
		index_t* where = upper_bound(begin(node), end(node) - 1, key);
		//把where后面的key后移一位，为key腾位置
		std::copy_backward(where, end(node), end(node) + 1);
		where->key = key;
		where->child = (where + 1)->child;
		(where + 1)->child = value;
		node.n++;
	}

	//将begin到end的父亲设置为parent
	void bplus_tree::reset_index_children_parent(index_t* begin, index_t* end,
		off_t parent) {
		// this function can change both internal_node_t and leaf_node_t's parent
		// field, but we should ensure that:
		// 1. sizeof(internal_node_t) <= sizeof(leaf_node_t)
		// 2. parent field is placed in the beginning and have same size
		internal_node_t node;
		while(begin != end) {
			map(&node, begin->child);
			node.parent = parent;
			unmap(&node, begin->child, SIZE_NO_CHILDREN);
			++begin;
		}
	}

	//返回键值为key的叶子结点父节点internal_node_t偏移量
	off_t bplus_tree::search_index(const key_t& key) const {
		off_t org = meta.root_offset;
		int height = meta.height;
		while(height > 1) {
			internal_node_t node;
			map(&node, org); //将org对应的内容赋值给node
			index_t* i = upper_bound(begin(node), end(node) - 1, key); //在node中查找第一个>=key的下标
			org = i->child; //向下查找
			--height;
		}

		return org;
	}

	//找到偏移为index的结点通过key返回寻找到的下一层结点的偏移量
	off_t bplus_tree::search_leaf(off_t index, const key_t& key) const {
		internal_node_t node;
		map(&node, index);

		index_t* i = upper_bound(begin(node), end(node) - 1, key);
		return i->child;
	}

	//offset是叶子结点node的偏移量，next是创建的node的兄弟结点
	template<class T>
	void bplus_tree::node_create(off_t offset, T* node, T* next) {
		//将next插入在同一父节点下的node后面
		next->parent = node->parent; //node的父亲就是next的父亲
		next->next = node->next;
		next->prev = offset;
		node->next = alloc(next);
		//更新原(node->next)->prev
		if(next->next != 0) {
			T old_next;
			map(&old_next, next->next, SIZE_NO_CHILDREN);
			old_next.prev = node->next;
			unmap(&old_next, next->next, SIZE_NO_CHILDREN);
		}
		unmap(&meta, OFFSET_META);
	}

	//prev是前一个结点，node是待删除结点
	template<class T>
	void bplus_tree::node_remove(T* prev, T* node) {
		unalloc(node, prev->next); //meta.internal_node_num--;或meta.leaf_node_num--;
		prev->next = node->next;
		if(node->next != 0) {
			T next;
			map(&next, node->next, SIZE_NO_CHILDREN);
			next.prev = node->prev;
			unmap(&next, node->next, SIZE_NO_CHILDREN);
		}
		unmap(&meta, OFFSET_META);
	}

	//初始化B+树
	void bplus_tree::init_from_empty() {
		//初始化meta
		bzero(&meta, sizeof(meta_t));
		meta.order = BP_ORDER;
		meta.value_size = sizeof(value_t);
		meta.key_size = sizeof(key_t);
		meta.height = 1;
		meta.slot = OFFSET_BLOCK;

		//初始化根结点
		internal_node_t root;
		root.next = root.prev = root.parent = 0;
		meta.root_offset = alloc(&root);

		//初始化空的叶子结点
		leaf_node_t leaf;
		leaf.next = leaf.prev = 0;
		leaf.parent = meta.root_offset;
		meta.leaf_offset = root.children[0].child = alloc(&leaf);

		unmap(&meta, OFFSET_META); //将meta写入db.bin
		unmap(&root, meta.root_offset); //将根节点写入db.bin
		unmap(&leaf, root.children[0].child); //将最左端的叶结点写入db.bin
	}

}
