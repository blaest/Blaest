List_createList(){
	auto liststruct;

	liststruct = malloc(4);

	liststruct[List_Array] = malloc(10);
	liststruct[List_Size] = 0;
	liststruct[List_Capacity] = 10;

	return liststruct;
}

List_resizeList(list, size){
	auto array, capacity, nextPtr, index, newArray, fmt[4];
	
	array = list[List_Array];
	capacity = list[List_Capacity];
	nextPtr = list[List_Size];

	newArray = malloc(size);

	index = 0;
	while(index < nextPtr){

		newArray[index] = array[index];
		index += 1;
	}
	
	free(array);
	list[List_Array] = newArray;
	list[List_Capacity] = size;
}

List_add(list, element){
	auto array, nextPtr, listCapacity;


	nextPtr = list[List_Size];
	listCapacity = list[List_Capacity];

	if(nextPtr >= listCapacity){
		List_resizeList(list, listCapacity + 10);
	}
	
	array = list[List_Array];
	
	array[nextPtr] = element;

	nextPtr += 1;
	array[nextPtr] = 0;


	list[List_Size] = nextPtr;

}

List_remove(list, index){
	auto array, size, elem;

	array = list[List_Array];
	size = list[List_Size];

	elem = array[index];
	while(index < size - 1){
		array[index] = array[index + 1];
		index += 1;
	}

	list[List_Size] = size - 1;

	return elem;
}

List_get(list, index){
	auto array;
	
	array = list[List_Array];

	if(index >= list[List_Size]){
		return 0;
	}

	return array[index];
}

List_clear(list){
	auto array;

	array = list[List_Array];
	free(array);
	
	list[List_Array] = malloc(10);
    list[List_Size] = 0;
    list[List_Capacity] = 10;
}

List_size(list){
	return list[List_Size];
}

List_free(list){
    auto array;
    array = list[List_Array];
    free(array);
    free(list);
}

List_Array 0;
List_Size 1;
List_Capacity 2;
