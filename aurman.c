#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <math.h>

#include <jansson.h>
#include <curl/curl.h>


typedef struct options{
	int status;
	int help;
	int search;
	int info;
	int source;
	int install;
	int remove;
	int packages_size;
	char** packages;
} options;

typedef struct response {
	char* data;
	size_t size;
} response;


options parse_opts(int argc, char* argv[]);
size_t process_request(char* response_data, size_t size, size_t nmemb, void* userdata);
response aur_request(char* type, char* package_name);
void sortByPopularity(int array_size, float popularity[], int order[]);


int main(int argc, char* argv[]) {
	options parsed_opts = parse_opts(argc, argv);

	if (parsed_opts.status != 0) {
		printf("(Type 'aurman [-h / --help]' for options)\n");
		free(parsed_opts.packages);
		return parsed_opts.status;
	}

	if (parsed_opts.help) {
		printf("Usage: aurman [--options] [...]\n");
		printf("Options:\n");
		printf("    aurman [-h / --help]                    Print this message\n");
		printf("    aurman [-S / --search] <package>        Search for given package\n");
		printf("    aurman [-I / --info] <package>          Get info for given package\n");
		printf("    aurman [-s / --source] <package(s)>     Source git files for given package(s)\n");
		printf("    aurman [-i / --install] <package(s)>    Build and install given package(s)\n");
		printf("    aurman [-r / --remove] <package(s)>     Remove git files for given package(s)\n");
		printf("\n");
		printf("--search, --info, --remove cannot be run alongwith other commands\n");
		printf("\n");

		return 0;
	}

	if (parsed_opts.search) {
		response res = aur_request("search", parsed_opts.packages[0]);
		json_t* root = json_loads(res.data, 0, NULL);

		json_t* results = json_object_get(root, "results");
		const int resultcount = json_integer_value(json_object_get(root, "resultcount"));

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
	else if (parsed_opts.info) {
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

		json_decref(root);
		free(res.data);
	}
	else if (parsed_opts.remove) {
		for (int i = 0; i < parsed_opts.packages_size; i++) {
			int cmd_len = 30 + (strlen(parsed_opts.packages[i])*1) + 1;
			char cmd[cmd_len];
			snprintf(cmd, cmd_len, "(cd $HOME/.aurman/ && rm -rf %s)", parsed_opts.packages[i]);
			system(cmd);
		}
		printf("Files removed!\nNote: Package must be uninstalled via pacman\n");
	}
	else {
		if (parsed_opts.source) {
			for (int i = 0; i < parsed_opts.packages_size; i++) {
				int cmd_len = 115 + (strlen(parsed_opts.packages[i])*4) + 1;
				char cmd[cmd_len];
				snprintf(cmd, cmd_len, "[ -d $HOME/.aurman/%s ] && (cd $HOME/.aurman/%s && git pull) || git clone https://aur.archlinux.org/%s.git $HOME/.aurman/%s", parsed_opts.packages[i], parsed_opts.packages[i], parsed_opts.packages[i], parsed_opts.packages[i]);
				system(cmd);
			}
		}
		if (parsed_opts.install) {
			for (int i = 0; i < parsed_opts.packages_size; i++) {
				int cmd_len = 35 + (strlen(parsed_opts.packages[i])*1) + 1;
				char cmd[cmd_len];
				snprintf(cmd, cmd_len, "(cd $HOME/.aurman/%s/ && makepkg -si)", parsed_opts.packages[i]);
				system(cmd);
			}
		}
	}

	free(parsed_opts.packages);
	return 0;
}


options parse_opts(int argc, char* argv[]) {
	options parsed_opts = {0, 0, 0, 0, 0, 0, 0, 0, NULL};
	static struct option long_options[] = {
		{"help",	no_argument, NULL, 'h'},
		{"search",	no_argument, NULL, 'S'},
		{"info",	no_argument, NULL, 'I'},
		{"source",	no_argument, NULL, 's'},
		{"install",	no_argument, NULL, 'i'},
		{"remove",	no_argument, NULL, 'r'},
	};

	int opt;
	int option_index = 0;
	int options_count = 0;
	while((opt = getopt_long(argc, argv, "hSIsir", long_options, &option_index)) != -1) {
		options_count++;
		if (opt == 'h')
			parsed_opts.help = 1;
		else if (opt == 'S')
			parsed_opts.search = 1;
		else if (opt == 'I')
			parsed_opts.info = 1;
		else if (opt == 's')
			parsed_opts.source = 1;
		else if (opt == 'i')
			parsed_opts.install = 1;
		else if (opt == 'r')
			parsed_opts.remove = 1;
		else if (opt == '?') {
			break;
		}
	}

	parsed_opts.packages_size = argc - optind;
	parsed_opts.packages = malloc(parsed_opts.packages_size * sizeof(char*));
	for (int i = optind; i < argc; i++)
		parsed_opts.packages[i-optind] = argv[i];

	if (parsed_opts.help == 1) {}
	else if (options_count == 0) { 
		fprintf(stderr, "Error: No options selected\n");
		parsed_opts.status = 1;
	}
	else if (opt == '?') {
		parsed_opts.status = 1;
	}
	else if ((parsed_opts.search || parsed_opts.info || parsed_opts.remove) && options_count != 1) {
		fprintf(stderr, "Error: --search, --info, --remove cannot be run alongwith other commands\n");
		parsed_opts.status = 2;
	}
	else if (parsed_opts.packages_size == 0) {
		fprintf(stderr, "Error: Package(s) not mentioned\n");
		parsed_opts.status = 3;
	}
	else if (parsed_opts.search || parsed_opts.info) {
		if (parsed_opts.packages_size > 1) {
			fprintf(stderr, "Error: --search, --info requires 1 package\n");
			parsed_opts.status = 3;
		}
		else if (strlen(parsed_opts.packages[0]) < 3) {
			fprintf(stderr, "Error: Package must have at least 3 characters\n");
			parsed_opts.status = 4;
		}
	}

	return parsed_opts;
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

	int url_len = 34 + strlen(type) + strlen(package_name) + 1;
	char url[url_len];
	snprintf(url, url_len, "https://aur.archlinux.org/rpc/v5/%s/%s", type, package_name);

	curl_easy_setopt(curl, CURLOPT_URL, url);
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
