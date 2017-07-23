#ifndef SEAFILE_CLIENT_UTILS_JSON_UTILS_H

#include <jansson.h>
#include <QString>

// A convenient class to access jasson `json_t` struct
class Json {
public:
    Json(const json_t *root = 0);

    QString getString(const char *name) const;
    qint64 getLong(const char *name) const;
    bool getBool(const char *name) const;
    Json getObject(const char *name) const;

private:
    const json_t *json_;
};


#if !defined(json_object_foreach)
#define json_object_foreach(object, key, value) \
    for(key = json_object_iter_key(json_object_iter(object)); \
        key && (value = json_object_iter_value(json_object_key_to_iter(key))); \
        key = json_object_iter_key(json_object_iter_next(object, json_object_key_to_iter(key))))
#endif

#if !defined(json_object_foreach_safe)
#define json_object_foreach_safe(object, n, key, value)     \
    for(key = json_object_iter_key(json_object_iter(object)), \
            n = json_object_iter_next(object, json_object_key_to_iter(key)); \
        key && (value = json_object_iter_value(json_object_key_to_iter(key))); \
        key = json_object_iter_key(n), \
            n = json_object_iter_next(object, json_object_key_to_iter(key)))
#endif

#if !defined(json_object_foreach)
#define json_array_foreach(array, index, value) \
	for(index = 0; \
		index < json_array_size(array) && (value = json_array_get(array, index)); \
		index++)
#endif

#endif  // SEAFILE_CLIENT_UTILS_JSON_UTILS_H
