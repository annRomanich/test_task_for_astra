#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define		FILE_PATH		"example.json"
#define		MAX_NAME_LEN	150

enum Parsing_mode {
	NAME_MODE,
	VALUE_MODE,
	CHILD_MODE
};

typedef struct Node
{
	GtkTreeIter  self_iter;
	GtkTreeIter* parent_iter;
	struct Node* next;
	struct Node* child;

	// needs for lazy parsing
	// number of first symbol to parse
	int start;

	// number of last symbol to parse
	int stop;

} Node;

struct Node node_new(
	GtkTreeStore *tree_store,
	GtkTreeIter *parent,
	char* text,
	int start,
	int stop
) {

	GtkTreeIter self;

	Node node = {
		self,
		parent,
		NULL,
		NULL,
		start,
		stop
	};

	gtk_tree_store_append(tree_store, &(node.self_iter), parent);
	gtk_tree_store_set(tree_store, &(node.self_iter), 0, text, 1, FALSE, -1);

	return node;
}

struct Node* node_new_p(
	GtkTreeStore *tree_store,
	GtkTreeIter *parent,
	char* text,
	int start,
	int stop
) {

	GtkTreeIter self;
	Node *new_node = (Node*) malloc(sizeof(Node));
	new_node->self_iter = self;
	new_node->parent_iter = parent;
	new_node->next = NULL;
	new_node->child = NULL;
	new_node->start = start;
	new_node->stop = stop;

	gtk_tree_store_append(tree_store, &(new_node->self_iter), parent);
	gtk_tree_store_set(tree_store, &(new_node->self_iter), 0, text, 1, FALSE, -1);

	return new_node;
}

void append_child(
	struct Node* node,
	GtkTreeStore *tree_store,
	char* text,
	int start,
	int stop
) {

	struct Node* new_node = node_new_p(tree_store, &(node->self_iter), text, start, stop);

	if (node->child == NULL) {
		node->child = new_node;
		// printf("test append node->child == NULL\n");
	}
	else {
		struct Node* last = node->child;
		while (last->next)
			last = last->next;
		last->next = new_node;
		// printf("test append\n");
	}
}

void append_next(
	struct Node* node,
	GtkTreeStore *tree_store,
	char* text,
	int start,
	int stop
) {
	struct Node new_nd = node_new(tree_store, NULL, text, start, stop);
	Node *new_node = (Node*) malloc(sizeof(Node));
	new_node = &new_nd;
	if (node->next == NULL)
		node->next = new_node;
	else {
		struct Node* next = node->next;
		while (next->next)
			next = next->next;
		next->next = (Node*) malloc(sizeof(Node));
		next->next = new_node;
	}
}

struct Node top_node;

void check_children(
  GtkTreeModel* model,
  GtkTreeIter* iter,
  gboolean checked
){
	if (gtk_tree_model_iter_has_child(model, iter)){
		GtkTreeIter child_iter;
		gtk_tree_model_iter_children(model, &child_iter, iter);
		do {
			gtk_tree_store_set(GTK_TREE_STORE(model), &child_iter, 1, checked, -1);
			check_children(model, &child_iter, checked);
		} while (gtk_tree_model_iter_next(model, &child_iter));
	}
}

struct Node* get_next_node(struct Node* node, int n)
{
	if (n == 0){	
		// printf("get_next_node_working_1\n");
		return node;
	}
	else
	{
		// printf("get_next_node_working_2\n");
		if (node)
			return get_next_node(node->next, n-1);
		else {
			printf("ERR: haven't next node!!!\n");
			return NULL;
		}
	}
		
}

struct Node* get_node_by_path(struct Node* top_node, GtkTreePath *path, int index_num)
{
	printf("get_node_by_path: %d\n", gtk_tree_path_get_indices(path)[index_num]);
	if (top_node==NULL)
		printf("top_node is NULL\n");
	if (top_node->child==NULL)
		printf("child is NULL\n");
	struct Node* node;
	int depth = gtk_tree_path_get_depth(path);
	if (index_num==0) {
		node = get_next_node(top_node, gtk_tree_path_get_indices(path)[index_num]);
	} else {
		node = get_next_node(top_node->child, gtk_tree_path_get_indices(path)[index_num]);
	}
	if (node){
		if (index_num == depth-1) {
			return node;
		} else {
			return get_node_by_path(node, path, index_num+1);
		}
	} else {
		printf("ERR: node by given path %s doesn't exist!\n", gtk_tree_path_to_string(path));
		return NULL;
	}
}

struct Node parse_level(
	struct Node* node, 
	GtkTreeStore *tree_store, 
	int start, 
	int stop)
{
	FILE* file = fopen(FILE_PATH, "r");
	fseek(file, 0, SEEK_END);
	fseek(file, start, SEEK_SET);
	long file_size = stop - start;

	printf("Size: %ld\n", file_size);

	char* buff 		= (char*)malloc(1);
	char* name_buff = (char*)malloc(MAX_NAME_LEN);
	char* value_buff = (char*)malloc(MAX_NAME_LEN);

	// brackets counter
	int counter = 0;
	int 
		name_counter = 0,
		value_counter = 0;
	bool 
		name = false,
		value = false;
	int 
		child_start, 
		child_stop;

	enum Parsing_mode mode = NAME_MODE;

	struct Node first_node;
	bool first_node_inited = false;

	struct Node* current_node = &first_node;

	// cycled character-by-character analysis
	for(int i=0; i<file_size; i++) {
		// printf("test 11\n");
		fread(buff, sizeof(char), 1, file);
		char symbol = *buff;

		// "NAME": "VALUE",
		// "NAME": (CHILD) { 
		//     "NAME": "VALUE",
		//     "NAME": "VALUE"
		// }

		if(mode == NAME_MODE)
		{
			// printf("NAME_MODE\n");
		
			if(symbol == '"'){	
				name = !name;
				// printf("test 7\n");

				// means end of a name
				if (name == false) {
					printf("Name: ");
					for (int j=0; j < name_counter; j++)
						printf("%c", name_buff[j]);
					printf("\n");
				}
			}
			else if(name){
				// printf("test 8\n");
				name_buff[name_counter] = symbol;
				name_counter++;
			}
			if(symbol == ':')
			{
				// printf("test 9\n");
				mode = VALUE_MODE;
			}
			// printf("test 10\n");
		}
		if(mode == VALUE_MODE)
		{		
			// printf("VALUE_MODE\n");		
			if (symbol == '{') {
				mode = CHILD_MODE;
			} 
			else if (symbol == '"') {
				value = !value;

				// means end of a name
				if (value == false) {
					printf("Value: ");
					for (int j=0; j < value_counter; j++){
						printf("%c", value_buff[j]);
					}
					printf("\n");
					printf("%d %d\n", name_counter, value_counter);
					char* str = (char*)malloc(name_counter+value_counter+3);
					for (int j=0; j < name_counter; j++){
						str[j] = name_buff[j];
					}
					str[name_counter] = ':';
					str[name_counter+1] = ' ';
					for (int j=0; j < value_counter; j++){
						str[name_counter+j+2] = value_buff[j];
					}
					str[name_counter+2+value_counter] = '\0';
					// printf("%s\n", value_buff);
					if (node == NULL){
						if (!first_node_inited){
							first_node = node_new(
								tree_store, 
								NULL, 
								str, 
								-1, -1
							);
							first_node_inited = true;
							Node *new_node = (Node*) malloc(sizeof(Node));
							new_node = &first_node;
							current_node = new_node;
							// printf("test 1");
						}
						else {
							append_next(current_node, tree_store, str, -1, -1);
							current_node = current_node->next;
							// printf("test 2");
						}
					}
					else {
						append_child(node, tree_store, str, -1, -1);
						// printf("test 5\n");
					}
					name_counter = 0;
					value_counter = 0;
				} 
			}	
			else if(value){
				value_buff[value_counter] = symbol;
				value_counter++;
			}
			if(symbol == ',' && value == false)
			{
				mode = NAME_MODE;
			}	
		}
		if(mode == CHILD_MODE)
		{
			// printf("CHILD_MODE\n");	
			if (symbol == '{') {
				if (counter == 0){
					if (node)
						printf("child: %d", i+node->start);
					else
						printf("child: %d", i);
					child_start = i;
				}
				counter++;
			}
			if (symbol == '}'){
				counter--;
				if (counter == 0){
					child_stop = i;
					if (node)
						printf("-%d\n", i+node->start);
					else
						printf("-%d\n", i);
					mode = NAME_MODE;
					char* str = (char*)malloc(name_counter+1);
					for (int j=0; j < name_counter; j++){
						str[j] = name_buff[j];
					}
					str[name_counter] = '\0';
					if (node == NULL){
						if (!first_node_inited){
							first_node = node_new(
								tree_store, 
								NULL, 
								str, 
								child_start, 
								child_stop
							);
							first_node_inited = true;
							Node *new_node = (Node*) malloc(sizeof(Node));
							new_node = &first_node;
							current_node = new_node;
							// printf("test 3");
						}
						else {
							append_next(
								current_node, 
								tree_store, 
								str, 
								child_start, 
								child_stop
							);
							current_node = current_node->next;
							// printf("test 4");
						}
					}
					else {
						append_child(
							node, 
							tree_store, 
							str, 
							child_start+node->start,
							child_stop+node->start
						);
					}
					name_counter = 0;
				}
			}
		}
	}
	// printf("test 12\n");
	if (node)
		return *(node->child);
	return first_node;
}

void checkbox_toggled(GtkCellRendererToggle *renderer, gchar *path_str, gpointer user_data)
{
	GtkTreeModel *model = GTK_TREE_MODEL(user_data);
	GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
	GtkTreeIter iter;

	// Get the iter corresponding to the selected path
	gtk_tree_model_get_iter(model, &iter, path);

	printf("toggle %s %d\n\n", path_str, gtk_tree_model_iter_n_children (model, &iter));

	// Toggle the value of the checkbox in the model
	gboolean checked;
	gtk_tree_model_get(model, &iter, 1, &checked, -1);
	checked = !checked;
	gtk_tree_store_set(GTK_TREE_STORE(model), &iter, 1, checked, -1);

	check_children(model, &iter, checked);

	// Clean up
	gtk_tree_path_free(path);
}

// void row_clicked(GtkTreeView *tree_view, gchar *path_str, gpointer user_data)
void row_expanded (
  GtkTreeView* self,
  GtkTreeIter* iter,
  GtkTreePath* path,
  gpointer user_data
)
{
	GtkTreeModel *model = GTK_TREE_MODEL(user_data);
	GtkTreeStore *tree_store = GTK_TREE_STORE(user_data);
	
	Node* node = get_node_by_path(&top_node, path, 0);
	
	printf(
		"row_expanded %s \t %d \t %d \n\n", 
		gtk_tree_path_to_string(path), 
		node->start, 
		node->stop
	);

	node = node->child;
	do {
		if (node->start != -1) {
			parse_level(node, tree_store, node->start, node->stop);
		}
		node = node->next;
	} while (node);
}

void parse_all(struct Node* node, GtkTreeStore *tree_store){
	Node* current = node;
	do {		
		// printf("parse_all_do %d %d\n", current->start, current->stop);
		if (current->start != -1) {
			// printf("parse_level \n");
			struct Node tn = parse_level(current, tree_store, current->start, current->stop);
			Node* top_node = (Node*) malloc(sizeof(Node));
			top_node = &tn;
			parse_all(top_node, tree_store);
		}
		current = current->next;
	} while (current != NULL);
}

long get_file_len() {
	FILE* file = fopen(FILE_PATH, "r");
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	fclose(file);
	return file_size;
}

int main(int argc, char *argv[])
{
	// Initialize GTK
	gtk_init(&argc, &argv);

	// Create the main window
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Checkbox List Example");
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	gtk_widget_set_size_request(window, 900, 600);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	// Create a list store model
	GtkTreeStore *tree_store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_BOOLEAN);

	// struct Node top_node = NULL;
	top_node = parse_level(NULL, tree_store, 0, get_file_len());
	Node* current_node = &top_node;
	do {
		if (current_node->start != -1) {
			parse_level(current_node, tree_store, current_node->start, current_node->stop);
		}
		current_node = current_node->next;
	} while (current_node);
	
	printf("\n");

	// Create a tree view
	GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);

	// Create a cell renderer for the checkbox
	GtkCellRenderer *cell_renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(cell_renderer, "toggled", G_CALLBACK(checkbox_toggled), tree_store);

	// Add the checkbox renderer to the first column
	GtkTreeViewColumn *column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, cell_renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, cell_renderer, "active", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

	// Add the text renderer to the second column
	cell_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Item", cell_renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
	g_signal_connect(tree_view, "row-expanded", G_CALLBACK(row_expanded), tree_store);

	// Create a scrollable container and add the tree view to it
	GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scrolled_window), tree_view);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
	GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	// Add the scrollable container to the main window
	gtk_container_add(GTK_CONTAINER(window), scrolled_window);

	// Show all the widgets
	gtk_widget_show_all(window);

	// Run the GTK main loop
	gtk_main();

	return 0;
}
