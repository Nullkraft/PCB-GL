
struct step_model_instance {
  double x;
  double y;
  double rotation;
};


struct step_model {
  const char *filename;
  GList *instances;
  double ox;
  double oy;
  double oz;
  double ax;
  double ay;
  double az;
  double rx;
  double ry;
  double rz;
};

struct step_model *step_model_to_shape_master (const char *filename);
