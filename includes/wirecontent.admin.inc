<?php

/**
 * @file
 * Administration pages provided by EType Wire module.
 */


/**
 * Menu callback for admin/content/wirecontent. Displays all Wire Content on
 * the site.
 *
 * @return string
 */
function etype_wire_admin_content() {

  global $base_url;

  $pattern = '/http:\\/\\/(www)*\./i';
  $sub = '';
  $url = preg_replace($pattern, $sub, $base_url);

  /* cluster */
  $string = '';
  $etype_wire_cluster = variable_get('etype_wire_cluster');
  if (!empty ($etype_wire_cluster)) {
    $tmp = [];
    foreach ($etype_wire_cluster as $k => $v) {
      if (strlen($v) > 1) {
        $tmp[] = " cluster REGEXP '$v' ";
      }
    }
    if (count($tmp) > 0) {
      $string = implode($tmp, ' OR ');
      $string = ' AND ( ' . $string . ' ) ';
    }
  }

  /* sorting */
  if (isset($_GET['order'])) {
    switch ($_GET['order']) {
      case 'Paper';
        $order = 'site_name';
        break;

      case 'Published';
        $order = 'created';
        break;

      default:
        $order = 'site_name';
    }
    $sort = $_GET['sort'];
  }
  else {
    $order = 'site_name';
    $sort = 'DESC';
  }

  /* connect to wire db */
  $dbHandle = _etype_wire_connect();
  $sql = "
SELECT * FROM `node` n
LEFT JOIN `node_site` ns ON n.nid = ns.original_nid
WHERE (
  n.site NOT LIKE \"%$url%\"
  AND (ns.site_base is NULL OR ns.site_base NOT LIKE \"%$url%\")
  $string
)
ORDER BY $order $sort
";

  $stmt = $dbHandle->prepare($sql);
  $stmt->execute();
  $stmt->setFetchMode(PDO::FETCH_OBJ);
  $wire_nodes = $stmt->fetchAll();
  return theme('wire_admin_content', ['wire_nodes' => $wire_nodes]);
}

/**
 * Generate a list of all wire content available on this site.
 */
function theme_wire_admin_content($variables) {

  $nodes = $variables['wire_nodes'];

  /* sortable header */
  $header = [
    t('Title'),
    ['data' => t('Paper'), 'field' => 'site_name'],
    ['data' => t('Published'), 'field' => 'created'],
    t('Operations'),
  ];

  $rows = [];
  foreach ($nodes as $node) {
    $rows[] = [
      l($node->title, $node->site . '/node/' . $node->nid, ["target" => "_blank"]),
      $node->site_name,
      format_date($node->created),
      l(t('Add'), 'admin/wirecontent/' . $node->nid . '/' .
        urlencode($node->site_name) . '/add'),
    ];
  }

  if (empty($rows)) {
    $message = t('No wire content available.');
    $rows[] = [
      ['data' => $message, 'colspan' => 4],
    ];
  }

  $per_page = 50;
  $current_page = pager_default_initialize(count($rows), $per_page);
  $chunks = array_chunk($rows, $per_page, TRUE);
  // Show the appropriate items from the list
  $output = theme('table', ['header' => $header, 'rows' => $chunks[$current_page]]);
  $output .= theme('pager', array('quantity',count($rows)));
  return $output;
}

/**
 * @return array
 * site settings form
 */
function etype_wire_site_admin() {

  if (_etype_wire_check_connection() === false) {

    $form['intro'] = array(
      '#type' => 'item',
      '#title' => '',
      '#markup' => '<p>Please add wire database connection information to settings.php.</p>'
    );
    return $form;

  } else {

    $form = [];

    $dbHandle = _etype_wire_connect();
    $sql = "SELECT `data` FROM `settings`";
    $stmt = $dbHandle->prepare($sql);
    $stmt->execute();
    $stmt->setFetchMode(PDO::FETCH_OBJ);
    $res = $stmt->fetchAll();
    $data = unserialize($res[0]->data);

    if (!empty($data['cluster'])) {

      $etype_wire_cluster_options = $data['cluster'];
      $first_key = key($etype_wire_cluster_options);
      /* get current setting */
      $etype_wire_cluster_default = variable_get('etype_wire_cluster', $etype_wire_cluster_options[$first_key]);

      $form['cluster'] = array(
        '#type' => 'fieldset',
        '#title' => t('Group'),
        '#collapsible' => TRUE,
        '#collapsed' => FALSE,
      );

      $form['cluster']['etype_wire_cluster'] = [
        '#title' => t('Site Group'),
        '#multiple' => TRUE,
        '#description' => t("The group that this site belongs to. Sites can belong to one or more groups and will only show wire content from checked groups."),
        '#weight' => '1',
        '#type' => 'checkboxes',
        '#options' => $etype_wire_cluster_options,
        '#default_value' => $etype_wire_cluster_default,
      ];

    }

    $form['settings'] = array(
      '#type' => 'fieldset',
      '#title' => t('Export Settings'),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
    );

    /* Content Type to export */
    $node_types = node_type_get_types();
    $types = array();
    foreach ($node_types as $item) {
      $types[$item->type] = $item->name;
    }
    $tmp = variable_get('etype_wire_content_type');
    $default_type = (empty($tmp))? 'article': $tmp;

    $form['settings']['etype_wire_content_type'] = array(
      '#type' => 'select',
      '#title' => t('Choose which content type to import and export'),
      '#default_value' => variable_get('etype_wire_content_type', $default_type),
      '#options' => $types,
    );

    /* Body export options */
    $format_options = [
      'html' => "HTML",
      'plain' => "Plain",
    ];

    $form['settings']['etype_wire_text_format'] = array(
      '#type' => 'radios',
      '#title' => t('Body text export format'),
      '#default_value' => variable_get('etype_wire_text_format', 'html'),
      '#checked' => 'checked',
      '#options' => $format_options,
    );

    $form['settings']['etype_wire_export_number'] = array(
      '#type' => 'select',
      '#options' => [0=>0, 5=>5, 10=>10, 15=>15, 20=>20, 25=>25, 30=>30, 35=>35, 40=>40, 45=>45, 50=>50, 'unlimited' => 'Unlimited'],
      '#default_value' => variable_get('etype_wire_export_number', '10'),
      '#title' => t('Number of stories to export'),
    );

    $form['settings']['etype_wire_export_since'] = array(
      '#type' => 'select',
      '#options' => [0=>0, 1=>1, 2=>2, 3=>3, 4=>4, 5=>5, 6=>6, 7=>7, 8=>8, 9=>9, 10=>10, 15=>15, 20=>20],
      '#default_value' => variable_get('etype_wire_export_since', '0'),
      '#title' => t('Or... number of days from which to export stories (overrides above)'),
    );

    $form['settings']['etype_wire_export_order'] = array(
      '#type' => 'select',
      '#options' => ['created'=>'Published','changed'=>'Updated'],
      '#default_value' => variable_get('etype_wire_export_order', 'created'),
      '#title' => t('Export order'),
    );

    $form['settings']['etype_wire_use_cron'] = array(
      '#title' => t('Enable export by cron job.'),
      '#type' => 'checkbox',
      '#default_value' => variable_get('etype_wire_use_cron', 0),
    );

    $form['#validate'][] = 'etype_wire_site_admin_validate';

    return system_settings_form($form);

  }
}

function etype_wire_site_admin_validate($form, &$form_state) {
  if ((int)$form_state['values']['etype_wire_export_since'] > 0) {
    $form_state['values']['etype_wire_export_number'] = 0;
  }
}

/**
 * Global Settings for the Groups
 * @return array
 */
function etype_wire_global_admin() {

  if (_etype_wire_check_connection() === false) {

    $form['intro'] = [
      '#type' => 'item',
      '#title' => '',
      '#markup' => '<p>Please add wire database connection information to settings.php.</p>'
    ];
    return $form;

  } else {

    $form = [];
    $text = '';
    $data = [];

    $dbHandle = _etype_wire_connect();
    $sql = "SELECT `data` FROM `settings`";
    $stmt = $dbHandle->prepare($sql);
    $stmt->execute();
    $stmt->setFetchMode(PDO::FETCH_OBJ);

    $res = $stmt->fetchAll();
    if (isset($res[0]->data)) {
      $data = unserialize($res[0]->data);
      if (!empty($data['cluster'])) {
        $etype_wire_cluster_options = $data['cluster'];
        foreach ($etype_wire_cluster_options as $k => $v) {
          $text .= "$k|$v\n";
        }
      }
    }

    $form['cluster'] = array(
      '#type' => 'fieldset',
      '#title' => t('Group'),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
    );

    $form['cluster']['etype_wire_cluster_setup'] = array(
      '#type' => 'textarea',
      '#title' => t('Group Options'),
      '#description' => t('Add or remove group options for all sites. Options should be on one line, with machine_name and name separated by |.'),
      '#default_value' => $text,
      '#required' => true,
    );

    $form['sections'] = array(
      '#type' => 'fieldset',
      '#title' => t('Section Tags'),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
    );

    $form['sections']['etype_wire_section_setup'] = array(
      '#type' => 'textfield',
      '#title' => t('Sections or Categories'),
      '#description' => t('Article section tags to match using regex when exporting nodes to the wire database. Separated by |.'),
      '#default_value' => $data['sections'],
    );

    $form['sections']['etype_wire_taxonomy'] = array(
      '#type' => 'textfield',
      '#title' => t('Taxonomy Field'),
      '#description' => t('The name of the field used to describe article sections or categories.'),
      '#default_value' => $data['taxonomy'],
    );

    $form['#submit'][] = 'etype_wire_global_admin_submit';

    return system_settings_form($form);
  }

}

/**
 * Make serialized array of option to store in wire database
 * @param $form
 * @param $form_state
 */
function etype_wire_global_admin_submit($form, &$form_state) {

  $data = [];
  $arr = [];
  $string = $form['cluster']['etype_wire_cluster_setup']["#value"];
  $split = preg_split("/\n/", $string);
  if (count($split) > 0)
  foreach($split as $item) {
    $tmp = explode("|", $item);
    $machine_name = trim($tmp[0]);
    if (!empty($machine_name)) {
      $name = trim($tmp[1]);
      $arr[$machine_name] = $name;
    }
  }

  $data['cluster'] = $arr;
  $data['sections'] = $form['sections']['etype_wire_section_setup']["#value"];
  $data['taxonomy'] = $form['sections']['etype_wire_taxonomy']["#value"];

  $serialized = serialize($data);
  $dbHandle = _etype_wire_connect();
  $sql = "SELECT COUNT(*) as c FROM `settings`";
  $stmt = $dbHandle->prepare($sql);
  $stmt->execute();
  $res = $stmt->fetchAll();
  if ($res[0]['c'] == 0) {
    $sql = "INSERT INTO `settings` set `data` = :data";
  } else {
    $sql = "UPDATE `settings` set `data` = :data";
  }
  $stmt = $dbHandle->prepare($sql);
  $res = $stmt->execute([':data' => $serialized]);

  // remove form value, do not need to store it
  unset ($form['cluster']['etype_wire_cluster_setup']);
  unset ($form['sections']['etype_wire_section_setup']);
  unset ($form['sections']['etype_wire_taxonomy']);

}
