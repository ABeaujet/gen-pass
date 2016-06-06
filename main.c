#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <yaml.h>
#include <openssl/sha.h>

#define ASCII_START 32
#define ASCII_END	126
#define ASCII_RANGE (ASCII_END-ASCII_START+1) // +1 since inclusive
#define MIN(x, y) ( (x < y) ? x : y)
#define MAX(x, y) ( (x > y) ? x : y)

/**
  * Usage :
  * ./myPass [-drUlnXx] sitename.com [pass_size]
  * 
  * TODO: Implémenter une base de donnée des changements de noms des sites.
  */

#define TYPE_UPPER		0x01	// U
#define TYPE_LOWER		0x02	// l
#define TYPE_NUMBER		0x04	// n
#define TYPE_SPECIAL	0x08	// s (U+n = X, l+n = x)
#define TYPE_ALL		(TYPE_UPPER | TYPE_LOWER | TYPE_SPECIAL | TYPE_NUMBER)

void print_usage(){
	printf("Usage : ./myPass [-drUlnXxs] [-L pass_size] sitename.com\n");
	printf("Note : (http(s)://)(www.) will be automatically trimmed.\n");
}

void print_hash(const unsigned char* in, size_t len){
	const unsigned char* c = in;
	for(unsigned int i = 0;i<len;i++)
		printf("%02x ", *c++);
	putchar('\n');
}

// % ASCII_RANGE + ASCII_START;		
char get_ascii_char(unsigned int pass_type, int index){
	static const char special[] = {'!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '<', '=', '>', '?', '@', '[', '\\', ']', '^', '_', '`', '{', '|', '}', '~' };
	#define special_size 32

	static const char upper[] = {'A','B','C','D','E','F','G','U','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};
	#define upper_size 26

	static const char lower[] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
	#define lower_size 26

	static const char num[] = {'0','1','2','3','4','5','6','7','8','9'};
	#define num_size 10

	static const char const * lists[] = { upper, lower, num, special, NULL};
	static const int const sizes[] = { upper_size, lower_size, num_size, special_size, 0};
	static const int list_count = 4;

	int modulo = 0;
	unsigned int pass_type2 = pass_type;
	for(int i =0;i<list_count;i++){
		if(pass_type2 & 0x01)
			modulo += sizes[i];
		pass_type2 >>= 1;
	}

	index %= modulo;

	for(int i =0;i<list_count;i++)
		if(pass_type & 1 << i)
			if(index < sizes[i]){
				printf("0x%02x:%c:%d:%d - ", lists[i][index], lists[i][index], i, index);
				return lists[i][index];
			}
			else
				index -= sizes[i];

	printf("What the fuck\n");
	return -128;
}

void gen_pass(char* master_pass, char* site_name, char* res, int out_len, unsigned int pass_type){
	char* c = site_name;
	if(res == NULL)
		res = calloc(out_len + 1, 1);
	int out_i = 0;
	unsigned char sha_site[33] = {'\0'};
	unsigned char sha_master[33] = {'\0'};
	memcpy(sha_site, SHA256(site_name, strlen(site_name), NULL), 32);
	memcpy(sha_master, SHA256(master_pass, strlen(master_pass), NULL), 32);
	printf("Domain hash : ");
	print_hash(sha_site, 32);
	printf("Master hash : ");
	print_hash(sha_master, 32);
	for(int i = 0;i<32;i++){
		out_i = i%out_len;
		printf("%d,", out_i);
		res[out_i] = get_ascii_char(pass_type, (res[out_i] + sha_site[i] + sha_master[i]));
	}
	//res[out_len] = '\0';
}

char* trim_url(char* in){
	// remove http, www
	const char * patterns[] = {"http://", "https://", "www.", NULL};
	char * out = in;
	for(int i = 0;patterns[i] != NULL;i++){
		const char* curr_pat = patterns[i];
		bool match = true;
		int j = 0;
		for(;match && curr_pat[j] && out[j];j++)
			if(curr_pat[j] != out[j])
				match = false;
		if(match)
			out += j;
	}
	// remove terminating forward slash
	size_t out_len = strlen(out);
	if(out[out_len - 1] == '/')
		out[out_len - 1] = '\0';
	return out;
}

int register_website_requirements(const char* domain, unsigned int pass_type){
	const char config_file[] = "./websites.lst";
	FILE* config = fopen(config_file, "a");
	if(config == NULL)
		return EXIT_FAILURE;
	fprintf(config, "%s:%u\n", domain, pass_type);
	fclose(config);
	return EXIT_SUCCESS;
}

int get_website_requirements(const char* domain, unsigned int *pass_type){
	const char config_file[] = "./websites.lst";
	FILE* config = fopen(config_file, "r+");
	if(config == NULL)
		goto fail;

	char* current_line = NULL;
	size_t current_line_size = 0;
	while(getline(&current_line, &current_line_size, config) >= 0){
		if(strlen(current_line) < 3) continue;

		char current_domain[100] = {'\0'};
		unsigned int current_requirement;
		char* delim = strchr(current_line, ':');
		if(delim == NULL){
			fclose(config);
			goto fail;
		}
		current_requirement = strtol(delim+1, NULL, 10);
		if(errno == EINVAL){
			fclose(config);
			goto fail;
		}
		memcpy(current_domain, current_line, delim-current_line);
		if(strcmp(domain, current_domain) == 0){
			*pass_type = current_requirement;
			fclose(config);
			return 0;
		}
	}
	fclose(config);
	return 1;

fail:
	fprintf(stderr, "Missing or corrupted syntax in config file.\n");
	return -1;
}

int remove_website_requirement(const char* domain){
	const char config_file[] = "./websites.lst";
	const char tmp_file[] = "./websites.lst.tmp";
	FILE* config = fopen(config_file, "r+");
	FILE* tmp = fopen(tmp_file, "w");
	if(config == NULL)
		goto fail;

	char* current_line = NULL;
	size_t current_line_size = 0;
	while(getline(&current_line, &current_line_size, config) >= 0){
		if(strlen(current_line) < 3) continue;

		char current_domain[100] = {'\0'};
		char* delim = strchr(current_line, ':');
		if(delim == NULL){
			fclose(config);
			goto fail;
		}
		memcpy(current_domain, current_line, delim-current_line);
		if(strcmp(domain, current_domain) != 0)
			fprintf(tmp, "%s", current_line);
	}
	fclose(config);
	fclose(tmp);
	if(rename(tmp_file, config_file) == 0)
		return EXIT_SUCCESS;
	else{
		fprintf(stderr, "Error while removing old website config.\n");
	}

fail:
	fprintf(stderr, "Missing or corrupted syntax in config file.\n");
	return EXIT_FAILURE;
}

int main(int argc, char* argv[]){

	if(argc < 2){
		print_usage();
		return EXIT_FAILURE;
	}

	bool debug = false;
	bool edit_website = false;
	int pass_len = 10;
	unsigned int pass_type = TYPE_ALL;
	bool default_config = true;
	#define IF_DEFAULT_RESET do{ if(default_config){ default_config = false; pass_type = 0; } }while(0);
	int index;
	int c;

	opterr = 0;
	while ((c = getopt (argc, argv, "dUlnXxsL:")) != -1){
		switch (c){
			case 'd':
				debug = true;
				break;
			case 'U':
				IF_DEFAULT_RESET
				pass_type |= TYPE_UPPER;
				edit_website = true;
				break;
			case 'l':
				IF_DEFAULT_RESET
				pass_type |= TYPE_LOWER;
				edit_website = true;
				break;
			case 'n':
				IF_DEFAULT_RESET
				pass_type |= TYPE_NUMBER;
				edit_website = true;
				break;
			case 'x':
				IF_DEFAULT_RESET
				pass_type |= TYPE_LOWER;
				pass_type |= TYPE_NUMBER;
				edit_website = true;
				break;
			case 'X':
				IF_DEFAULT_RESET
				pass_type |= TYPE_UPPER;
				pass_type |= TYPE_NUMBER;
				edit_website = true;
				break;
			case 's':
				IF_DEFAULT_RESET
				pass_type |= TYPE_SPECIAL;
				edit_website = true;
				break;
			case 'L':
				pass_len = strtol(optarg, NULL, 10);
				if(errno == EINVAL){
					fprintf(stderr, "Invalid password size specified.\n");
					print_usage();
					return EINVAL;
				}
				break;
			case '?':
				if (optopt == 'L')
					fprintf (stderr, "Option -%c requires an argument.\n", optopt);
				else if (isprint (optopt))
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				else
				fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				return 1;
			default:
				print_usage();
				return EINVAL;
		}
	}
	index = optind;
/*
	char domain[100] = {'\0'};
	strncpy(domain, argv[index], 100 - 1);
*/
	char *domain = trim_url(argv[index]);

	printf("Options : %u\n", pass_type);
	printf ("Domain : %s\n", domain);

	unsigned int old_pass_type;
	int err = get_website_requirements(domain, &old_pass_type);
	if(err == 0){
		if(edit_website && default_config == false){
			do{
				printf("replacing password type for website %s : %d -> %d ? (y/n) ", domain, old_pass_type, pass_type);
				c = getchar();
				putchar('\n');
			}while(c != 'y' && c != 'n');

			if(c == 'y'){
				if(	remove_website_requirement(domain) == EXIT_SUCCESS && 
					register_website_requirements(domain, pass_type) == EXIT_SUCCESS)
					printf("Website config updated : \n");
				else{
					printf("Error while updating website config. Aborting.\n");
					exit(EXIT_FAILURE);
				}
			}else
				printf("Change aborted.\n");
		}else{
			printf("Website already registered : ");
			pass_type = old_pass_type;
		}

		printf("Using password type : %u\n", pass_type);
	}
	else if(err == 1){
		if(register_website_requirements(domain, pass_type) == EXIT_SUCCESS)
			printf("Website config inseted\n");
		else{
			printf("Error while inserting website config. Aborting.\n");
			exit(EXIT_FAILURE);
		}
	}
	else
		return EXIT_FAILURE;

	char master_pass[100] = {'\0'};
	char* output = calloc(pass_len + 1, 1);

	while(strlen(master_pass) == 0)
		strcpy(master_pass, getpass("Master password : "));

	gen_pass(master_pass, domain, output, pass_len, pass_type);
	printf("Password : %s\n", output);

	return EXIT_SUCCESS;
}
