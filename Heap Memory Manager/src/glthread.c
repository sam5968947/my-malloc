#include "glthread.h"


/**
 * init glthread node
 */
void glthread_init(glthread_node_t* glthread){

    glthread->right = NULL;
    glthread->left = NULL;
}


/**
 * insert glthread node to the right of the current node
 */
void glthread_add(glthread_node_t* current, glthread_node_t* new){

    if(current == NULL || new == NULL){

        return;
    }

    new->right = current->right;
    new->left = current;

    if(new->right != NULL){

        new->right->left = new;
    }

    current->right = new;
}

/**
 * insert glthread node to the left of the current node
 */
void glthread_add_pre(glthread_node_t* current, glthread_node_t* new){

    if(current == NULL || new == NULL){

        return;
    }

    new->right = current;
    current->left = new;

    new->left = current->left;
    if(current->left != NULL){

        current->left->right = new;
    }
}


/**
 * insert node to the head of the list 
 */
void glthread_add_first(glthread_t* list, glthread_node_t* glnode){

    if(glnode == NULL || list == NULL){

        return;
    }

    glnode->right = list->head;
    glnode->left = NULL;
    
    if(glnode->right != NULL){

        glnode->right->left = glnode;
    }

    list->head = glnode; 
}


/**
 * insert node to the pq
 */ 
void glthread_priority_insert(glthread_node_t* base_glthread, 
                              glthread_node_t* new_glthread, 
                              int (*comp_fn)(void *, void *), 
                              int offset){

    assert(base_glthread);
    assert(new_glthread);

    glthread_init(new_glthread);

    if(IS_GLTHREAD_LIST_EMPTY(base_glthread)){

        glthread_add(base_glthread, new_glthread);
        return;
    }

    if(base_glthread->right && !base_glthread->right->right){

        if(comp_fn(GLTHREAD_GET_USER_DATA_FROM_OFFSET(base_glthread->right, offset), 
                GLTHREAD_GET_USER_DATA_FROM_OFFSET(new_glthread, offset)) == -1){

            glthread_add(base_glthread->right, new_glthread);
        }else{

            glthread_add(base_glthread, new_glthread);
        }
        return;
    }


    if(comp_fn(GLTHREAD_GET_USER_DATA_FROM_OFFSET(new_glthread, offset), 
            GLTHREAD_GET_USER_DATA_FROM_OFFSET(base_glthread->right, offset)) == -1){

        glthread_add(base_glthread, new_glthread);
        return;
    }

    glthread_node_t* cur = NULL, *pre = NULL;

    ITERATE_GLTHREAD_BEGIN(base_glthread, cur){
        if(comp_fn(GLTHREAD_GET_USER_DATA_FROM_OFFSET(new_glthread, offset), 
                GLTHREAD_GET_USER_DATA_FROM_OFFSET(cur, offset)) == -1){

            glthread_add(cur, new_glthread);
            return;
        }

        pre = cur;
    }ITERATE_GLTHREAD_END(base_glthread, cur)

    glthread_add(pre, new_glthread);
}


/**
 * delete specific node
 */
void glthread_remove(glthread_node_t* glnode){

    if(glnode == NULL){

        return;
    }

    if(glnode->left){

        glnode->left->right = glnode->right;
    }

    if(glnode->right){

        glnode->right->left = glnode->left;
    }

    glnode->left = NULL;
    glnode->right = NULL;

}


