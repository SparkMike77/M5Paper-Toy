#ifndef _FRAME_GROCERYLIST_H_
#define _FRAME_GROCERYLIST_H_

#include "frame_base.h"
#include "../epdgui/epdgui.h"
#include <vector>

struct GroceryItem {
    String id;
    String name;
    bool complete;
};

class Frame_GroceryList : public Frame_Base {
public:
    Frame_GroceryList();
    ~Frame_GroceryList();
    int init(epdgui_args_vector_t &args);
    void exit();
    void CommitChanges();

private:
    void FetchAndBuildList();
    void ClearList();

    std::vector<GroceryItem> _items;
    std::vector<EPDGUI_Switch*> _key_items;
    EPDGUI_Button *_key_commit;
};

#endif //_FRAME_GROCERYLIST_H_
