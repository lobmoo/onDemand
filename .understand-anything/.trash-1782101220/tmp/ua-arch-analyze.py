#!/usr/bin/env python3
"""Architecture analysis script for onDemand project."""

import json
import sys
import os
from collections import defaultdict, Counter
from pathlib import PurePosixPath

def main():
    input_path = sys.argv[1]
    output_path = sys.argv[2]

    with open(input_path, 'r') as f:
        data = json.load(f)

    file_nodes = data['fileNodes']
    import_edges = data.get('importEdges', [])
    all_edges = data.get('allEdges', [])

    node_map = {n['id']: n for n in file_nodes}

    # A. Directory Grouping - group by meaningful project directory
    # For this C++ project, group by the first 2 path segments (e.g., core/ondemand, common/fastdds_wrapper)
    # but collapse .gensrc into a separate group
    def get_dir_group(filepath):
        parts = filepath.split('/')
        if parts[0] == '.claude':
            return '.claude'
        if parts[0] == '.understand-anything':
            return '.understand-anything'
        if parts[0] == '.github':
            return '.github'
        if parts[0] == 'thirdparty':
            if len(parts) >= 3:
                return 'thirdparty/' + parts[1]
            return 'thirdparty'
        if len(parts) >= 3 and parts[2] == '.gensrc':
            return parts[0] + '/' + parts[1] + '/.gensrc'
        if len(parts) >= 3:
            return parts[0] + '/' + parts[1]
        if len(parts) == 2:
            # Single file at top-level dir (e.g., config/logConfig.json, core/CMakeLists.txt)
            # Group by top-level directory
            return parts[0]
        return '(root)'

    dir_groups = defaultdict(list)
    for n in file_nodes:
        group = get_dir_group(n['filePath'])
        dir_groups[group].append(n['id'])

    # B. Node Type Grouping
    node_type_groups = defaultdict(list)
    for n in file_nodes:
        node_type_groups[n['type']].append(n['id'])

    # C. Import Adjacency
    file_fan_out = defaultdict(int)
    file_fan_in = defaultdict(int)
    for e in import_edges:
        file_fan_out[e['source']] += 1
        file_fan_in[e['target']] += 1

    # Build group-to-group import matrix
    node_to_group = {}
    for n in file_nodes:
        node_to_group[n['id']] = get_dir_group(n['filePath'])

    inter_group_imports = defaultdict(int)
    for e in import_edges:
        src_group = node_to_group.get(e['source'])
        tgt_group = node_to_group.get(e['target'])
        if src_group and tgt_group:
            key = (src_group, tgt_group)
            inter_group_imports[key] += 1

    inter_group_list = []
    for (frm, to), count in sorted(inter_group_imports.items(), key=lambda x: -x[1]):
        inter_group_list.append({"from": frm, "to": to, "count": count})

    # D. Cross-Category Dependency Analysis
    cross_cat = defaultdict(int)
    for e in all_edges:
        src_type = node_map.get(e['source'], {}).get('type', 'unknown')
        tgt_type = node_map.get(e['target'], {}).get('type', 'unknown')
        key = (src_type, tgt_type, e.get('type', 'unknown'))
        cross_cat[key] += 1

    cross_cat_list = []
    for (frm, to, etype), count in sorted(cross_cat.items(), key=lambda x: -x[1]):
        cross_cat_list.append({"fromType": frm, "toType": to, "edgeType": etype, "count": count})

    # E. Intra-Group Import Density
    intra_density = {}
    for group, members in dir_groups.items():
        member_set = set(members)
        internal = 0
        total = 0
        for e in import_edges:
            src_in = e['source'] in member_set
            tgt_in = e['target'] in member_set
            if src_in or tgt_in:
                total += 1
                if src_in and tgt_in:
                    internal += 1
        density = internal / total if total > 0 else 0
        intra_density[group] = {
            "internalEdges": internal,
            "totalEdges": total,
            "density": round(density, 3)
        }

    # F. Pattern Matching
    dir_pattern_map = {
        'routes': 'api', 'api': 'api', 'controllers': 'api', 'endpoints': 'api',
        'handlers': 'api', 'serializers': 'api', 'controller': 'api', 'routers': 'api',
        'blueprints': 'api',
        'services': 'service', 'core': 'service', 'lib': 'service', 'domain': 'service',
        'logic': 'service', 'internal': 'service', 'signals': 'service', 'composables': 'service',
        'mailers': 'service', 'jobs': 'service', 'channels': 'service',
        'models': 'data', 'db': 'data', 'data': 'data', 'persistence': 'data',
        'repository': 'data', 'entities': 'data', 'entity': 'data', 'migrations': 'data',
        'components': 'ui', 'views': 'ui', 'pages': 'ui', 'ui': 'ui', 'layouts': 'ui',
        'screens': 'ui',
        'middleware': 'middleware', 'plugins': 'middleware', 'interceptors': 'middleware',
        'guards': 'middleware',
        'utils': 'utility', 'helpers': 'utility', 'common': 'utility', 'shared': 'utility',
        'tools': 'utility', 'templatetags': 'utility', 'pkg': 'utility',
        'config': 'config', 'constants': 'config', 'env': 'config', 'settings': 'config',
        'management': 'config', 'commands': 'config',
        '__tests__': 'test', 'test': 'test', 'tests': 'test', 'spec': 'test', 'specs': 'test',
        'types': 'types', 'interfaces': 'types', 'schemas': 'types', 'contracts': 'types',
        'dtos': 'types', 'dto': 'types', 'request': 'types', 'response': 'types',
        'hooks': 'hooks',
        'store': 'state', 'state': 'state', 'reducers': 'state', 'actions': 'state',
        'slices': 'state',
        'assets': 'assets', 'static': 'assets', 'public': 'assets',
        'cmd': 'entry', 'bin': 'entry',
        'docs': 'documentation', 'documentation': 'documentation', 'wiki': 'documentation',
        'sample': 'example', 'examples': 'example', 'demo': 'example',
        'deploy': 'infrastructure', 'deployment': 'infrastructure', 'infra': 'infrastructure',
        'infrastructure': 'infrastructure',
        '.github': 'ci-cd', '.gitlab': 'ci-cd', '.circleci': 'ci-cd',
        'k8s': 'infrastructure', 'kubernetes': 'infrastructure', 'helm': 'infrastructure',
        'charts': 'infrastructure',
        'terraform': 'infrastructure', 'tf': 'infrastructure', 'docker': 'infrastructure',
        'sql': 'data', 'database': 'data', 'schema': 'data',
        '.gensrc': 'auto-generated', 'gensrc': 'auto-generated',
    }

    pattern_matches = {}
    for group in dir_groups:
        # Check directory name patterns
        # Get the last meaningful segment
        parts = group.split('/')
        matched = False
        for p in reversed(parts):
            p_lower = p.lower()
            if p_lower in dir_pattern_map:
                pattern_matches[group] = dir_pattern_map[p_lower]
                matched = True
                break
        if not matched:
            # Check file-level patterns based on tags
            members = dir_groups[group]
            member_nodes = [node_map[m] for m in members if m in node_map]
            all_tags = set()
            for n in member_nodes:
                all_tags.update(n.get('tags', []))
            all_paths = [n.get('filePath', '') for n in member_nodes]

            if any(t in all_tags for t in ['third-party', 'thirdparty', 'vendored']):
                pattern_matches[group] = 'third-party'
            elif any(t in all_tags for t in ['auto-generated', 'idl-generated']):
                pattern_matches[group] = 'auto-generated'
            elif any(t in all_tags for t in ['test', 'testing']):
                pattern_matches[group] = 'test'
            elif any(t in all_tags for t in ['documentation']):
                pattern_matches[group] = 'documentation'
            elif any(t in all_tags for t in ['example', 'sample']):
                pattern_matches[group] = 'example'
            elif any(t in all_tags for t in ['configuration', 'build-system']):
                pattern_matches[group] = 'config'
            elif any(t in all_tags for t in ['dds', 'dds-idl']):
                pattern_matches[group] = 'dds'
            elif any(t in all_tags for t in ['core']):
                pattern_matches[group] = 'service'
            elif any(t in all_tags for t in ['logging']):
                pattern_matches[group] = 'utility'

    # G. Deployment Topology
    infra_files = []
    has_docker = False
    has_compose = False
    has_k8s = False
    has_terraform = False
    has_ci = False

    for n in file_nodes:
        fp = n['filePath'].lower()
        base = os.path.basename(fp)
        if 'dockerfile' in base:
            has_docker = True
            infra_files.append(n['filePath'])
        elif 'docker-compose' in base:
            has_compose = True
            infra_files.append(n['filePath'])
        elif any(k in fp for k in ['k8s/', 'kubernetes/', 'helm/', 'charts/']):
            has_k8s = True
            infra_files.append(n['filePath'])
        elif base.endswith('.tf') or base.endswith('.tfvars'):
            has_terraform = True
            infra_files.append(n['filePath'])
        elif '.github/workflows/' in fp or '.gitlab-ci' in fp or 'jenkinsfile' in base:
            has_ci = True
            infra_files.append(n['filePath'])

    deployment_topology = {
        "hasDockerfile": has_docker,
        "hasCompose": has_compose,
        "hasK8s": has_k8s,
        "hasTerraform": has_terraform,
        "hasCI": has_ci,
        "infraFiles": infra_files
    }

    # H. Data Pipeline Detection
    schema_files = []
    migration_files = []
    data_model_files = []
    api_handler_files = []

    for n in file_nodes:
        fp = n['filePath']
        tags = n.get('tags', [])
        base = os.path.basename(fp).lower()

        if base.endswith('.idl') or 'schema' in base or base.endswith('.graphql') or base.endswith('.proto'):
            schema_files.append(fp)
        if 'migration' in fp.lower():
            migration_files.append(fp)
        if any(t in tags for t in ['data-model']):
            data_model_files.append(fp)
        if any(t in tags for t in ['dds-topic']):
            api_handler_files.append(fp)

    data_pipeline = {
        "schemaFiles": schema_files,
        "migrationFiles": migration_files,
        "dataModelFiles": data_model_files,
        "apiHandlerFiles": api_handler_files
    }

    # I. Documentation Coverage
    doc_groups = set()
    for n in file_nodes:
        if n['type'] == 'document':
            group = get_dir_group(n['filePath'])
            doc_groups.add(group)

    # Also check README in each group
    for group in dir_groups:
        for m in dir_groups[group]:
            n = node_map.get(m, {})
            if n.get('name', '').lower().startswith('readme'):
                doc_groups.add(group)

    total_groups = len(dir_groups)
    groups_with_docs = len(doc_groups)
    undocumented = [g for g in dir_groups if g not in doc_groups]

    doc_coverage = {
        "groupsWithDocs": groups_with_docs,
        "totalGroups": total_groups,
        "coverageRatio": round(groups_with_docs / total_groups, 3) if total_groups > 0 else 0,
        "undocumentedGroups": undocumented
    }

    # J. Dependency Direction
    direction_map = defaultdict(lambda: {'forward': 0, 'backward': 0})
    for (frm, to), count in inter_group_imports.items():
        if frm != to:
            key = tuple(sorted([frm, to]))
            if frm == key[0]:
                direction_map[key]['forward'] += count
            else:
                direction_map[key]['backward'] += count

    dependency_direction = []
    for (a, b), counts in direction_map.items():
        if counts['forward'] > counts['backward']:
            dependency_direction.append({"dependent": a, "dependsOn": b})
        elif counts['backward'] > counts['forward']:
            dependency_direction.append({"dependent": b, "dependsOn": a})

    # K. File Stats
    files_per_group = {g: len(ids) for g, ids in dir_groups.items()}
    node_type_counts = {t: len(ids) for t, ids in node_type_groups.items()}

    file_stats = {
        "totalFileNodes": len(file_nodes),
        "filesPerGroup": files_per_group,
        "nodeTypeCounts": node_type_counts
    }

    # Sort fan-in/fan-out for output
    fan_in_sorted = dict(sorted(file_fan_in.items(), key=lambda x: -x[1])[:30])
    fan_out_sorted = dict(sorted(file_fan_out.items(), key=lambda x: -x[1])[:30])

    result = {
        "scriptCompleted": True,
        "directoryGroups": {g: ids for g, ids in sorted(dir_groups.items())},
        "nodeTypeGroups": {t: ids for t, ids in node_type_groups.items()},
        "crossCategoryEdges": cross_cat_list,
        "interGroupImports": inter_group_list,
        "intraGroupDensity": intra_density,
        "patternMatches": pattern_matches,
        "deploymentTopology": deployment_topology,
        "dataPipeline": data_pipeline,
        "docCoverage": doc_coverage,
        "dependencyDirection": dependency_direction,
        "fileStats": file_stats,
        "fileFanIn": fan_in_sorted,
        "fileFanOut": fan_out_sorted
    }

    with open(output_path, 'w') as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    print(f"Analysis complete. {len(file_nodes)} files, {len(dir_groups)} groups, {len(import_edges)} import edges.")
    print("Groups:", sorted(dir_groups.keys()))
    print("Pattern matches:", pattern_matches)
    print("Deployment:", deployment_topology)

if __name__ == '__main__':
    main()
