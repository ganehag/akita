#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <modbus/modbus.h>

#define CONFIG_FILE_PATH "/etc/config/akita"
#define DEFAULT_INTERVAL 30
#define DEFAULT_TIMEOUT 180
#define MAX_LINE_LENGTH 256
#define MAX_OPTION_LENGTH 64

typedef struct {
  char host[MAX_OPTION_LENGTH];
  int port;
  int slave_id;
  int register_address;
  int interval;
  int timeout;
  char script_dir[MAX_OPTION_LENGTH];
} akita_config_t;

typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR } LogLevel;

volatile sig_atomic_t flag = 0;

const char *level_strings[] = {"DEBUG", "INFO", "WARNING", "ERROR"};
akita_config_t config;

void sig_handler(int signum) { flag = 1; }

void log_p(LogLevel level, const char *format, ...) {
  time_t raw_time;
  struct tm *time_info;
  char timestamp[20];

  time(&raw_time);
  time_info = localtime(&raw_time);

  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time_info);
  fprintf(stderr, "[%s] [%s] ", timestamp, level_strings[level]);

  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}

void execute_shell_script(const char *shell_script_path) {
  char buffer[128];
  FILE *fp;
  char *script_name;

  fp = popen(shell_script_path, "r");
  if (fp == NULL) {
    perror("popen");
    return;
  }

  script_name = basename((char *)shell_script_path);
  while (fgets(buffer, sizeof(buffer) - 1, fp) != NULL) {
    log_p(LOG_INFO, "%s: %s", script_name, buffer);
  }

  if (pclose(fp) == -1) {
    perror("pclose");
  }
}

int filter_executable(const char *script_dir, const struct dirent *entry) {
  struct stat st;
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/%s", script_dir, entry->d_name);
  if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
    return 1;
  }
  return 0;
}

int compare_alphabetical(const struct dirent **a, const struct dirent **b) {
  return strcmp((*a)->d_name, (*b)->d_name);
}

void execute_shell_scripts_in_directory(const char *script_dir) {
  struct dirent **namelist;
  int n;

  n = scandir(script_dir, &namelist, NULL, compare_alphabetical);
  if (n == -1) {
    log_p(LOG_ERROR, "Failed to open script directory: %s\n", script_dir);
    return;
  }

  for (int i = 0; i < n; i++) {
    if (filter_executable(script_dir, namelist[i])) {
      char script_path[PATH_MAX];
      snprintf(script_path, sizeof(script_path), "%s/%s", script_dir,
               namelist[i]->d_name);

      log_p(LOG_INFO, "Executing script: %s\n", namelist[i]->d_name);
      execute_shell_script(script_path);
    }

    free(namelist[i]);
  }
  free(namelist);
}

int read_modbus_data(akita_config_t *config, uint16_t *current_value) {
  modbus_t *ctx = modbus_new_tcp(config->host, config->port);
  if (ctx == NULL) {
    perror("Unable to create Modbus context");
    return -1;
  }

  modbus_set_slave(ctx, config->slave_id);

  if (modbus_connect(ctx) == -1) {
    perror("Modbus connection failed");
    modbus_free(ctx);
    return -1;
  }

  int rc =
      modbus_read_registers(ctx, config->register_address, 1, current_value);
  if (rc != 1) {
    perror("Failed to read Modbus register");
    rc = -1;
  }

  modbus_close(ctx);
  modbus_free(ctx);
  return rc;
}

int parse_config(const char *config_file_path, akita_config_t *config) {
  FILE *file = fopen(config_file_path, "r");
  if (file == NULL) {
    return -1;
  }

  char line[MAX_LINE_LENGTH];
  char *key, *value;
  while (fgets(line, MAX_LINE_LENGTH, file) != NULL) {
    // Remove newline character if present
    if (line[strlen(line) - 1] == '\n') {
      line[strlen(line) - 1] = '\0';
    }

    // Skip empty lines and comments
    if (strlen(line) == 0 || line[0] == '#') {
      continue;
    }

    // Split line into key and value
    key = strtok(line, "=");
    value = strtok(NULL, "=");
    if (key == NULL || value == NULL) {
      fprintf(stderr, "Invalid config file format: %s\n", config_file_path);
      exit(EXIT_FAILURE);
    }

    // Parse key-value pairs
    if (strcmp(key, "host") == 0) {
      strncpy(config->host, value, MAX_OPTION_LENGTH - 1);
      config->host[MAX_OPTION_LENGTH - 1] = '\0';
    } else if (strcmp(key, "port") == 0) {
      config->port = atoi(value);
    } else if (strcmp(key, "slave_id") == 0) {
      config->slave_id = atoi(value);
    } else if (strcmp(key, "register_address") == 0) {
      config->register_address = atoi(value);
    } else if (strcmp(key, "interval") == 0) {
      config->interval = atoi(value);
    } else if (strcmp(key, "timeout") == 0) {
      config->timeout = atoi(value);
    } else if (strcmp(key, "script_dir") == 0) {
      strncpy(config->script_dir, value, MAX_OPTION_LENGTH - 1);
      config->script_dir[MAX_OPTION_LENGTH - 1] = '\0';
    } else {
      fprintf(stderr, "Unknown key in config file: %s\n", key);
      exit(EXIT_FAILURE);
    }
  }

  fclose(file);
  return 0;
}

int main(int argc, char *argv[]) {
  uint16_t last_value = 0, current_value = 0;
  int watchdog_triggered = 0;
  int opt;
  char *config_file_path = CONFIG_FILE_PATH;

  memset(&config, 0, sizeof(config));

  while ((opt = getopt(argc, argv, "c:")) != -1) {
    switch (opt) {
    case 'c':
      config_file_path = optarg;
      break;
    default: /* '?' */
      fprintf(stderr, "Usage: %s [-c config_file_path]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (parse_config(config_file_path, &config) != 0) {
    log_p(LOG_ERROR, "unable to parse config: %s\n", config_file_path);
    exit(EXIT_FAILURE);
  }

  // Ensure that we have some interval
  if (config.interval == 0) {
    config.interval = DEFAULT_INTERVAL;
  }
  if (config.timeout == 0) {
    config.timeout = DEFAULT_TIMEOUT;
  }

  if (config.timeout <= config.interval) {
    log_p(LOG_WARNING, "timeout (%i)s is lower than check interval (%i)s\n",
          config.timeout, config.interval);
    exit(EXIT_FAILURE);
  }

  signal(SIGALRM, sig_handler);

  // Arm the alarm
  alarm(config.timeout);

  while (1) {
    if (read_modbus_data(&config, &current_value) == 1) {
      if (current_value != last_value) {
        last_value = current_value;
        if (watchdog_triggered) {
          log_p(LOG_INFO, "watchdog re-established\n");
          watchdog_triggered = 0;
        }
        // Re-arm the alarm
        alarm(config.timeout);
      }
    }

    if (flag && !watchdog_triggered) {
      flag = 0;
      watchdog_triggered = 1;
      log_p(LOG_WARNING, "watchdog timeout, executing shell scripts\n");
      execute_shell_scripts_in_directory(config.script_dir);
    }

    sleep(config.interval);
  }

  return EXIT_SUCCESS;
}
