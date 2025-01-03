#pragma once

#include <map>

#include "compiler.h"

using namespace std;

class VM;

class ExecutionState {
public:
	typedef Compiler::mapping_type::value_type global_mapping_type;

	static const global_mapping_type& globalDefs(Compiler* cp);

	ExecutionState() {}

	void loadNames(Compiler* cp, Compiler::name_array& names);

	void cleanGlobalsMap(NativeBase* nb);

	void snapshot(VM* vm);

	void loadSnapshot(VM* vm);
	void copyObjectTree(Object**);

	Object* recursiveRegisterObject(Object*, ValuePool*);

	void registerPropertyMap(Object*, ValuePool*);

	auto& get_ref_map() {
		return globals;
	}

protected:

	struct ObjectReplcDetails {
		vector<Object**> places;
		pair<Value, Value>* props = NULL;
		size_t prop_count = 0;
		Object* replacement = NULL;

		ObjectReplcDetails(){}
		ObjectReplcDetails(Object**, decltype(props), const size_t&);

		void addPlace(Object** v) {
			places.push_back(v);
		}

		void replace();

		void buildProperties();
	};

	Object* addReplacement(Object**, decltype(ObjectReplcDetails::props), const size_t&);

#ifdef VM_SHOW_SNAPSHOTTING
	size_t scope = 0;
#endif

	size_t natives = 0;
	global_mapping_type globals;
	vector<Value> global_values;
	set<Object*> objects;

	//map<Object*, Object*> object_map;
	map<Object*, ObjectReplcDetails> object_replacements;
};