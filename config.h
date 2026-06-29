/*
 * Configuration Header for Cipher-Core
 */

#ifndef CONFIG_H
#define CONFIG_H

#define PROJECT_NAME "Cipher-Core"
#define PROJECT_VERSION "1.0.0"
#define BUILD_DATE __DATE__

/* Feature flags */
#define ENABLE_CRYPTO 1
#define ENABLE_NETWORK 1
#define ENABLE_THREADS 1
#define ENABLE_DATABASE 1
#define ENABLE_EVENT_LOOP 1

/* Limits */
#define MAX_MODULES 8
#define MAX_CONNECTIONS 64

#endif /* CONFIG_H */
