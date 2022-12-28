#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <math.h>

#include <jansson.h>
#include <curl/curl.h>


typedef struct options{
	char** packages;
	int packages_size;
	int search;
	int info;
	int source;
	int install;
	int remove;
	int help;
} options;

typedef struct response {
	char* data;
	size_t size;
} response;


char* join(int arr_size, char* arr[], char* join_val);
options parse_opts(int argc, char* argv[]);
int validate_options(options parsed_opts);
size_t process_request(char* response_data, size_t size, size_t nmemb, void* userdata);
response aur_request(char* type, char* package_name);
void sortByPopularity(int array_size, float popularity[], int order[]);


int main(int argc, char* argv[]) {
	options parsed_opts = parse_opts(argc, argv);

	int valid = validate_options(parsed_opts);
	if (valid != 0) {
		printf("(Type [-h / --help] for options)\n");
		free(parsed_opts.packages);
		return valid;
	}


	if (parsed_opts.help) {
		printf("Usage: aurman [--options] [...]\n");
		printf("Options:\n");
		printf("[-h / --help]\n    Print this message\n");
		printf("[-S / --search] <package>\n    Search for given package\n");
		printf("[-I / --info] <package>\n    Get info for given package\n");
		printf("[-s / --source] <package(s)>\n    Source git files for given package(s)\n");
		printf("[-i / --install] <package(s)>\n    Build and install given package(s)\n");
		printf("[-r / --remove] <package(s)>\n    Remove git files for given package(s)\n");

		return 0;
	}

	if (parsed_opts.search) {
		response res = aur_request("search", parsed_opts.packages[0]);
		json_t* root = json_loads(res.data, 0, NULL);

		const int resultcount = json_integer_value(json_object_get(root, "resultcount"));

		json_t* results = json_object_get(root, "results");

		int order[resultcount];
		float popularity[resultcount];
		for (int i = 0; i < resultcount; i++) {
			json_t* result = json_array_get(results, i);

			order[i] = i;
			popularity[i] = json_number_value(json_object_get(result, "Popularity"));
		}
		sortByPopularity(resultcount, popularity, order);

		for (int i = 0; i < resultcount; i++) {
			json_t* result = json_array_get(results, order[i]);

			const char* maintainer = json_string_value(json_object_get(result, "Maintainer"));
			const char* name = json_string_value(json_object_get(result, "Name"));
			const char* version = json_string_value(json_object_get(result, "Version"));
			const char* description = json_string_value(json_object_get(result, "Description"));

			printf("%s/%s %s\n    %s\n", maintainer, name, version, description);
		}

		json_decref(root);
		free(res.data);
	}
	if (parsed_opts.info) {
		response res = aur_request("info", parsed_opts.packages[0]);
		json_t* root = json_loads(res.data, 0, NULL);

		json_t* results = json_object_get(root, "results");
		const int resultcount = json_integer_value(json_object_get(root, "resultcount"));

		if (resultcount == 1) {
			json_t* result = json_array_get(results, 0);
			const char* name = json_string_value(json_object_get(result, "Name"));
			const char* maintainer = json_string_value(json_object_get(result, "Maintainer"));
			const char* version = json_string_value(json_object_get(result, "Version"));
			const char* description = json_string_value(json_object_get(result, "Description"));
			const char* url = json_string_value(json_object_get(result, "URL"));
			const float popularity = json_number_value(json_object_get(result, "Popularity"));
			const int numvotes = json_integer_value(json_object_get(result, "NumVotes"));

			printf("Maintainer\t: %s\nName\t\t: %s\nVersion\t: %s\nDescription\t: %s\nURL\t\t: %s\nPopularity\t: %f\nVotes\t\t: %d\n", maintainer, name, version, description, url, popularity, numvotes);
		} 
		else {
			printf("Package not found\n");
		}

		free(res.data);
	}
	if (parsed_opts.source) {
		for (int i = 0; i < parsed_opts.packages_size; i++) {
			char* source = join(8, (char* []){"[ -d $HOME/.aurman/", parsed_opts.packages[i], " ] && (cd $HOME/.aurman/", parsed_opts.packages[i], " && git pull) || git clone https://aur.archlinux.org/", parsed_opts.packages[i], ".git $HOME/.aurman/", parsed_opts.packages[i]}, "");
			system(source);
			free(source);
		}
	}
	if (parsed_opts.install) {
		for (int i = 0; i < parsed_opts.packages_size; i++) {
			char* install = join(3, (char* []){"(cd $HOME/.aurman/", parsed_opts.packages[i], "/ && makepkg -si)"}, "");
			system(install);
			free(install);
		}
	}
	if (parsed_opts.remove) {
		char* packages = join(parsed_opts.packages_size, parsed_opts.packages, " ");
		char* remove = join(3, (char* []){"(cd $HOME/.aurman/ && rm -rf ", packages, ")"}, "");
		system(remove);
		free(packages);
		free(remove);
		printf("Files removed!\nNote: Package must be uninstalled via pacman\n");
	}

	free(parsed_opts.packages);
	return 0;
}


char* join(int arr_size, char* arr[], char* join_val) {
	int str_length = 1 + (sizeof(join_val) * arr_size);
	for (int i = 0; i < arr_size; i++)
		str_length += strlen(arr[i]);

	char* str = malloc(sizeof(char) * str_length);
	strcpy(str, arr[0]);
	for (int i = 1; i < arr_size; i++)
		strcat(strcat(str, join_val), arr[i]);

	return str;
}

options parse_opts(int argc, char* argv[]) {
	options parsed_opts = {NULL, 0, 0, 0, 0, 0};
	static struct option long_options[] = {
		{"search",	no_argument, NULL, 'S'},
		{"info",	no_argument, NULL, 'I'},
		{"source",	no_argument, NULL, 's'},
		{"install",	no_argument, NULL, 'i'},
		{"remove",	no_argument, NULL, 'r'},
		{"help",	no_argument, NULL, 'h'}
	};

	int opt;
	int option_index = 0;
	while((opt = getopt_long(argc, argv, "SIsirh", long_options, &option_index)) != -1) {
		if (opt == 'S')
			parsed_opts.search = 1;
		else if (opt == 'I')
			parsed_opts.info = 1;
		else if (opt == 's')
			parsed_opts.source = 1;
		else if (opt == 'i')
			parsed_opts.install = 1;
		else if (opt == 'r')
			parsed_opts.remove = 1;
		else if (opt == 'h')
			parsed_opts.help = 1;
		else if (opt == '?') {
			printf("Unknown arg: %c\n", optopt);
			break;
		}
	}

	parsed_opts.packages_size = argc - optind;
	parsed_opts.packages = malloc(parsed_opts.packages_size * sizeof(char*));
	for (int i = 0; optind < argc; i++, optind++)
		parsed_opts.packages[i] = argv[optind];
	return parsed_opts;
}

int validate_options(options parsed_opts) {
	if (parsed_opts.help)
		return 0;
	if (!(parsed_opts.search || parsed_opts.info || parsed_opts.source || parsed_opts.install || parsed_opts.remove)) {
		printf("Error: No options selected\n");
		return 1;
	}
	if (parsed_opts.search && (parsed_opts.info || parsed_opts.source || parsed_opts.install || parsed_opts.remove)) {
		printf("Error: Cannot search and perform other functions in the same command\n");
		return 2;
	}
	if (parsed_opts.info && (parsed_opts.source || parsed_opts.install || parsed_opts.remove)) {
		printf("Error: Cannot get information and perform other functions in the same command\n");
		return 2;
	}
	if (parsed_opts.remove && (parsed_opts.source || parsed_opts.install)) {
		printf("Error: Cannot remove source(s) and perform other functions in the same command\n");
		return 2;
	}
	if (parsed_opts.packages_size == 0) {
		printf("Error: Package(s) not mentioned\n");
		return 3;
	}
	if (parsed_opts.search || parsed_opts.info) {
		if (parsed_opts.packages_size > 1) {
			printf("Error: Search requires 1 package\n");
			return 3;
		}
		if (strlen(parsed_opts.packages[0]) < 3) {
			printf("Error: Packages to search must have at least 3 characters\n");
			return 4;
		}
	}
	return 0;
}

size_t process_request(char* response_data, size_t size, size_t nmemb, void* userdata) {
	size_t realsize = size * nmemb;
	response* res = (response*)userdata;

	res->size += realsize;
	res->data = realloc((res->data), res->size + 1);
	strcat(res->data, response_data);

	return realsize;
}

response aur_request(char* type, char* package_name) {
	curl_global_init(CURL_GLOBAL_DEFAULT);

	CURL* curl = curl_easy_init();
	response res;
	res.data = malloc(sizeof(char));
	res.data[0] = '\0';
	res.size = 0;

	char* url = join(4, (char* []){"https://aur.archlinux.org/rpc/v5/", type, "/", package_name}, "");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	free(url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, process_request);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);

	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	curl_global_cleanup();

	return res;
}

void sortByPopularity(int length, float popularity[], int order[]) {
	if (length > 1) {
		int left_length = (int)floor((float)length / 2.0);
		float left_popularity[left_length];
		int left_order[left_length];
		for (int i = 0; i < left_length; i++) {
			left_popularity[i] = popularity[i];
			left_order[i] = order[i];
		}
		sortByPopularity(left_length, left_popularity, left_order);

		int right_length = (int)ceil((float)length / 2.0);
		float right_popularity[right_length];
		int right_order[right_length];
		for (int i = 0; i < right_length; i++) {
			right_popularity[i] = popularity[left_length + i];
			right_order[i] = order[left_length + i];
		}
		sortByPopularity(right_length, right_popularity, right_order);


		int left_counter = 0;
		int right_counter = 0;
		for (int i = 0; i < length; i++) {
			if (left_counter < left_length && (right_counter >= right_length || left_popularity[left_counter] >= right_popularity[right_counter])) {
				popularity[i] = left_popularity[left_counter];
				order[i] = left_order[left_counter];
				left_counter++;
			}
			else {
				popularity[i] = right_popularity[right_counter];
				order[i] = right_order[right_counter];
				right_counter++;
			}
		}
	}
}
